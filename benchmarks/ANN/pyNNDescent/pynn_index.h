// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <algorithm>
// #include "parlay/internal/group_by.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/random.h"
#include "common/geometry.h"
#include "../utils/cluster.h"
#include <random>
#include <set>
#include <math.h>

extern bool report_stats;

template<typename T>
struct pyNN_index{
	int K;
	unsigned d;
	float delta;
	using tvec_point = Tvec_point<T>;
	using fine_sequence = parlay::sequence<int>;
	// using slice_tvec = decltype(make_slice(parlay::sequence<tvec_point*>()));
	using edge = std::pair<int, int>;
	using labelled_edge = std::pair<edge, float>;
	using pid = std::pair<int, float>;
	using special_edge = std::pair<int, pid>; //this format makes it easier to use the group_by functions

	pyNN_index(int md, unsigned dim, float Delta) : K(md), d(dim), delta(Delta) {}

	static parlay::sequence<edge> naive_neighbors(parlay::sequence<tvec_point*> &v, parlay::sequence<size_t>& active_indices,
		unsigned dim, int maxK){
		size_t n = active_indices.size();
		int k = std::min(maxK, (int) (n-1));
		parlay::sequence<edge> edges(k*n);
		parlay::parallel_for(0, n, [&] (size_t i){
			parlay::sequence<labelled_edge> labelled_edges(n-1);
			parlay::parallel_for(0, n, [&] (size_t j){
				float dist = distance(v[active_indices[i]]->coordinates.begin(), v[active_indices[j]]->coordinates.begin(), dim);
				labelled_edge e = std::make_pair(std::make_pair(active_indices[i], active_indices[j]), dist);
				if(j<i) labelled_edges[j] = e;
				else if(j>i) labelled_edges[j-1] = e;
			});
			auto less = [&] (labelled_edge a, labelled_edge b) {return a.second < b.second;};
			auto sorted_edges = parlay::sort(labelled_edges);
			for(int j=0; j<k; j++) {
				edges[k*i+j] = sorted_edges[j].first;
			}
		});
		return edges;
	}

	parlay::sequence<int> nn_descent(parlay::sequence<tvec_point*> &v, parlay::sequence<int> &prev_changed){
		auto changed = parlay::tabulate(v.size(), [&] (size_t i) {return 0;});
		//find the edges in the reverse graph
		auto reverse_graph = reverse_edges(v);
		//tabulate all-pairs distances for the undirected graph
		parlay::sequence<parlay::sequence<special_edge>> grouped_labelled(v.size());
		parlay::parallel_for(0, reverse_graph.size(), [&] (size_t i){
			size_t index = reverse_graph[i].first;
			//only add edges if the index was changed on a previous round
			if(prev_changed[index] == 1){
				parlay::sequence<special_edge> edges;
				for(const int& j : v[index]->out_nbh) reverse_graph[i].second.push_back(j);
				for(const int& j : reverse_graph[i].second){
					for(const int& k : reverse_graph[i].second){
						if(j != k){
							float dist = distance(v[j]->coordinates.begin(), v[k]->coordinates.begin(), d);
							edges.push_back(std::make_pair(j, std::make_pair(k, dist)));
							edges.push_back(std::make_pair(k, std::make_pair(j, dist)));
						}
					}
				}
				grouped_labelled[index] = edges;
			}
		});
		parlay::parallel_for(0, v.size(), [&] (size_t i){
			if(grouped_labelled[i].size() == 0 && prev_changed[i] == 1){ 
				for(const int& j : v[i]->out_nbh){
					for(const int& k : v[i]->out_nbh){
						if(j != k){
							float dist = distance(v[j]->coordinates.begin(), v[k]->coordinates.begin(), d);
							grouped_labelled[i].push_back(std::make_pair(j, std::make_pair(k, dist)));
							grouped_labelled[i].push_back(std::make_pair(k, std::make_pair(j, dist)));
						}
					}
				}
			}
		});
		auto flat_labelled = parlay::flatten(grouped_labelled);
		auto grouped_by = parlay::group_by_key(flat_labelled);
		// update edges of each vertex based on the candidates in grouped_by
		parlay::parallel_for(0, grouped_by.size(), [&] (size_t i){
			size_t index = grouped_by[i].first;
			fine_sequence new_out(K);
			for(const int& j : v[index]->out_nbh) {
				float dist = distance(v[index]->coordinates.begin(), v[j]->coordinates.begin(), d);
				grouped_by[i].second.push_back(std::make_pair(j, dist));
			}
			auto less = [&] (pid a, pid b) {return a.second < b.second;};
			auto sorted_candidates = parlay::sort(grouped_by[i].second);
			int k=0;
			int j=0;
			while(k < K){
				if(parlay::find(new_out, sorted_candidates[j].first) != new_out.end()) j++; 
				else{
					new_out[k] = sorted_candidates[j].first; k++; j++;
				}
				
			}
			for(int j=0; j<K; j++) new_out[j] = sorted_candidates[j].first;
			for(const int& j: new_out){
				if(parlay::find(v[index]->out_nbh, j) == v[index]->out_nbh.end()){
					changed[index] = 1;
					v[index]->new_out_nbh = new_out;
					break;
				} 
			}
		});
		//finally, synchronize the new out-neighbors
		parlay::parallel_for(0, v.size(), [&] (size_t i){
			if(changed[i] == 1){
				v[i]->out_nbh = v[i]->new_out_nbh;
				v[i]->new_out_nbh.clear();
			}
		});
		return changed;
	}

	int nn_descent_wrapper(parlay::sequence<tvec_point*> &v){
		auto changed = parlay::tabulate(v.size(), [&] (size_t i) {return 1;});
		int rounds = 0;
		while(parlay::reduce(changed) >= delta*v.size()){
			std::cout << "Round " << rounds << std::endl; 
			parlay::sequence<int> new_changed = nn_descent(v, changed);
			// std::cout << "here1" << std::endl; 
			changed = new_changed;
			rounds ++;
			std::cout << parlay::reduce(changed) << " elements changed" << std::endl; 
		}
		std::cout << "descent converged in " << rounds << " rounds" << std::endl; 
		return rounds;
	}

	parlay::sequence<std::pair<int, parlay::sequence<int>>> reverse_edges(parlay::sequence<tvec_point*> &v){
		// parlay::sequence<std::pair<int, parlay::sequence<int>>> reverse_graph(v.size());
		parlay::sequence<parlay::sequence<edge>> grouped_edges(v.size());
		parlay::parallel_for(0, v.size(), [&] (size_t i){
			parlay::sequence<edge> uedges = parlay::tabulate((v[i]->out_nbh).size(), [&] (size_t j) {
				return std::make_pair(v[i]->out_nbh[j], (int) i);});
			grouped_edges[i] = uedges;
		});
		auto edges = parlay::flatten(grouped_edges);
		auto reverse_graph = parlay::group_by_key(edges);
		return reverse_graph;
	}

	void truncate_graph(parlay::sequence<tvec_point*> &v){
		parlay::parallel_for(0, v.size(), [&] (size_t i) {
			auto distances = parlay::tabulate(v[i]->out_nbh.size(), [&] (size_t j){
				float dist = distance(v[i]->coordinates.begin(), v[v[i]->out_nbh[j]]->coordinates.begin(), d);
				return std::make_pair(v[i]->out_nbh[j], dist);
			});
			auto less = [&] (pid a, pid b) {return a.second < b.second;};
			auto sorted = parlay::sort(distances);
			fine_sequence new_out = parlay::tabulate(K, [&] (size_t j){
				return sorted[j].first;
			});
			v[i]->out_nbh = new_out;
		});
	}

	void build_index(parlay::sequence<tvec_point*> &v, size_t cluster_size){
		clear(v);
		cluster<T> C(d);
		C.multiple_clustertrees(v, cluster_size, 20, &naive_neighbors, d, K);
		truncate_graph(v);
		nn_descent_wrapper(v);
	}

};
