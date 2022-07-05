#include <bits/stdc++.h>
using namespace std;

//#define DEBUG

#ifdef DEBUG
	#define	demand_path "../data/demand.csv"
	#define	site_bandwidth_path "../data/site_bandwidth.csv"
	#define	qos_path "../data/qos.csv"
	#define	config_path "../data/config.ini"
	#define	output_path "../data/solution.txt"
	#define MAX_TIME 5
#else
	#define	demand_path "/data/demand.csv"
	#define	site_bandwidth_path "/data/site_bandwidth.csv"
	#define	qos_path "/data/qos.csv"
	#define	config_path "/data/config.ini"
	#define	output_path "/output/solution.txt"
	#define MAX_TIME 290
#endif

ifstream file_demand;
ifstream file_site_bandwidth;
ifstream file_qos;
ifstream file_config;
ofstream file_output;

unordered_map<string, int> n2id_c;
vector<string> id2n_c;//id_c to name_C

unordered_map<string, int> n2id_s;
vector<string> id2n_s;//id_s to name

vector<int> site_bandwidth_vec;//id_s to band_max

int qos_constraint;//max_qos
int base_cost;//base_cost

int N_c, N_s, N_t;

class Demand_t
{
public:
	int stream_num = 0;
	vector<string> stream_name;//stream_id to stream_name
	vector<vector<int>> demand;//stream_id & id_c to demand
	vector<vector<int>> destination;//stream_id & id_c to tar_id_s
	vector<vector<int>> ans;//stream_id & id_c to tar_id_s (best destination)
};
vector<Demand_t> demand_t_vec;//t to all_demand

class Usedband_t_set
{
public:
	int big_size = 0;// N_t/20
	int small_size = 0;// N_t - N_t/20
	set<pair<int, int>, less<pair<int, int>>> big_set;
	set<pair<int, int>, greater<pair<int, int>>> small_set;

    bool is_free() {
		auto it = big_set.end();
		it--;
        return it->first == 0;
    }

    int get_score_band() {
		auto it = small_set.begin();
        return it->first;
    }

    int get_state(const pair<int, int> &usedband_t) {
        /*if (big_set.find(usedband_t) == big_set.end()) {//not find in big_set
			if (usedband_t.first == get_score_band()) {
				return 2;
			}
			else {
				return 3;
			}
		}
		return 1;*/

        if (big_set.find(usedband_t) != big_set.end())
            return 1;
        else {
            if (get_score_band() == usedband_t.first) {
                if (usedband_t.first == (*big_set.begin()).first) {
					return 1;
				} 
                else {
					return 2;
				}
            }
        }
        return 3;
    }

	void get_migrate_small_t_vec(vector<int> &migrate_t_vec) {
		auto it = small_set.begin();
		int score_band = it->first;
		if (score_band > 0) {
			while (1) {
				migrate_t_vec.emplace_back(it->second);
				if (it == small_set.end()) {
					break;
				}
				it++;
				if (it->first < score_band) {
					break;
				}
			}
		}
	}

	void get_migrate_big_t_vec(vector<int> &migrate_t_vec) {
		for (auto it: big_set) {
			if (it.first > 0) {
				migrate_t_vec.emplace_back(it.second);
			}
        }
	}

	void erase(const pair<int, int> &usedband_t) {
		big_set.erase(usedband_t);
		small_set.erase(usedband_t);
	}

	void emplace(const pair<int, int> &usedband_t) {
		if (small_set.size() < small_size) {
			small_set.emplace(usedband_t);
		}
		else {
			big_set.emplace(usedband_t);
		}

        auto it_small = small_set.begin();
        auto it_big = big_set.begin();

        if (it_small->first > it_big->first) {

			pair<int, int> p_small(it_small->first, it_small->second);
			pair<int, int> p_big(it_big->first, it_big->second);

			big_set.erase(p_big);
			big_set.insert(p_small);

			small_set.erase(p_small);
            small_set.insert(p_big);
        }

	}

};

void move(Demand_t &demand_t, vector<Usedband_t_set> &used_t_set, vector<vector<int>> &free_band, vector<vector<int>> &used_band,int t, int stream_id, int id_c, int id_s, int to_id_s) {
	int _demand = demand_t.demand[stream_id][id_c];

	demand_t.destination[stream_id][id_c] = to_id_s;

	used_t_set[to_id_s].erase(pair<int, int>(used_band[t][to_id_s], t));
	used_band[t][to_id_s] += _demand;
	free_band[t][to_id_s] -= _demand;
	used_t_set[to_id_s].emplace(pair<int, int>(used_band[t][to_id_s], t));

	used_t_set[id_s].erase(pair<int, int>(used_band[t][id_s], t));
	used_band[t][id_s] -= _demand;
	free_band[t][id_s] += _demand;
	used_t_set[id_s].emplace(pair<int, int>(used_band[t][id_s], t));
}

double cul_cost(int W,int C) {
	if (W <= base_cost) {
		return base_cost;
	}
	double temp = W - base_cost;
	temp = temp * temp / C;   
	temp += W;
	return temp;
}

int calculate_cost(vector<Usedband_t_set> &used_t_set) {
	double cost_sum = 0.5;
	for (int id_s = 0; id_s < N_s; id_s++) {
		if (used_t_set[id_s].big_set.size() != 0 && !used_t_set[id_s].is_free()) {
			cost_sum += cul_cost(used_t_set[id_s].get_score_band(), site_bandwidth_vec[id_s]);
		}
	}
	return cost_sum;
}

int min_cost = INT_MAX;

int t_once_max = 0;

int main() {

	time_t  start_time = time(NULL);

	{//read config.ini
		file_config.open(config_path);
		string temp;
		getline(file_config,temp);
		while (getline(file_config,temp)) {
			if (temp.size() > 0) {
				int pos = temp.find('=');
				if (temp.substr(0, pos) == "qos_constraint"){
					qos_constraint = atoi(temp.substr(pos + 1, temp.size() - pos).c_str());
				}
				else if (temp.substr(0, pos) == "base_cost"){
					base_cost = atoi(temp.substr(pos + 1, temp.size() - pos).c_str());
				}
			}
		}
		file_config.close();
	}
	{//read site_bandwidth.csv init id2n_s & site_bandwidth_vec
		file_site_bandwidth.open(site_bandwidth_path);
		string temp;
		getline(file_site_bandwidth,temp);
		while (getline(file_site_bandwidth,temp)) {
			if (temp.size() > 0) {
				int pos = temp.find(',');
				id2n_s.emplace_back(temp.substr(0, pos));
				site_bandwidth_vec.emplace_back(atoi(temp.substr(pos + 1, temp.size() - pos).c_str()));
			}
		}
		file_site_bandwidth.close();
		N_s = id2n_s.size();
	}
	{//read demand.csv  init id2n_c & demand
		file_demand.open(demand_path);
		string temp;
		file_demand >> temp;
		temp = temp.substr(16) + ',';
		string name_c;
		for (char &ch : temp) {
			if (ch == ',') {
				id2n_c.emplace_back(name_c);
				name_c.clear();
			}
			else {
				name_c += ch;
			}
		}
		N_c = id2n_c.size();

		N_t = 0;
		string last_time;
		while (getline(file_demand,temp)) {
			if (temp.size() >= 17) {
				string cur_time = temp.substr(0, 16);
				if (cur_time != last_time) {
					last_time = cur_time;
					demand_t_vec.emplace_back(Demand_t());
					N_t++;
				}

				temp = temp.substr(17, temp.size() - 18) + ',';
				int pos = temp.find(',');
				demand_t_vec[N_t - 1].stream_num++;
				demand_t_vec[N_t - 1].stream_name.emplace_back(temp.substr(0, pos));
				demand_t_vec[N_t - 1].demand.emplace_back(vector<int>(0));
				temp = temp.substr(pos + 1, temp.size() - pos);

				string band;
				for (char &ch : temp) {
					if (ch == ',') {
						demand_t_vec[N_t - 1].demand[demand_t_vec[N_t - 1].stream_num - 1].emplace_back(atoi(band.c_str()));
						band.clear();
					}
					else {
						band += ch;
					}
				}
			}
		}
		file_demand.close();
	}
	{//map string - id
		for (int id_c = 0; id_c < N_c; id_c++) {
			n2id_c[id2n_c[id_c]] = id_c; 
		}
		for (int id_s = 0; id_s < N_s; id_s++) {
			n2id_s[id2n_s[id_s]] = id_s; 
		}
	}
	vector<vector<bool>> qos(N_s, vector<bool>(N_c, false));//id_s & id_c to qos
	{//read qos.csv
		file_qos.open(qos_path);
		string temp;
		file_qos >> temp;
		temp = temp.substr(10) + ',';
		string qos_s_name;
		vector<int> qos_s_id;//qos_id to id_s
		for (char &ch : temp) {
			if (ch == ',') {
				qos_s_id.emplace_back(n2id_c[qos_s_name]);
				qos_s_name.clear();
			}
			else {
				qos_s_name += ch;
			}
		}
		while (getline(file_qos,temp)) {
			int i = 0;
			if (temp.size() > 0) {
				int j = -1;
				temp += ',';
				string qos_str;
				for (char &ch : temp) {
					if (ch == ',') {
						if (j >= 0) {
							if (atoi(qos_str.c_str()) < qos_constraint) {
								qos[i][qos_s_id[j]] = true;
							}
						}
						else {
							i = n2id_s[qos_str.c_str()];
						}
						qos_str.clear();
						j++;
					}
					else {
						qos_str += ch;
					}
				}
			}
		}
		file_qos.close();
	}
START:	
	srand(time(NULL));
	time_t  t_once_start = time(NULL);
	//initial demand_t_vec
	for (int t = 0; t < N_t; t++) {
		Demand_t &demand_t = demand_t_vec[t];
		if (demand_t.destination.size()!= 0) {
			demand_t.destination.clear();
		}
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			demand_t.destination.emplace_back(vector<int>(N_c, -1));
		}
	}
	//used & free band
	vector<vector<int>> free_band(N_t, site_bandwidth_vec);//t & id_s to free_band
	vector<vector<int>> used_band(N_t, vector<int>(N_s, 0));//t & id_s to used_band
	//connection map init
	vector<vector<int>> c_connected_to_s(N_s);
	for (int id_s = 0; id_s < N_s; id_s++) {
		for (int id_c = 0; id_c < N_c; id_c++) {
			if (qos[id_s][id_c]) {
				c_connected_to_s[id_s].emplace_back(id_c);
			}
		}
	}
	vector<vector<int>> s_connected_to_c(N_c);
	for (int id_c = 0; id_c < N_c; id_c++) {
		for (int id_s = 0; id_s < N_s; id_s++) {
			if (qos[id_s][id_c]) {
				s_connected_to_c[id_c].emplace_back(id_s);
			}
		}
	}
	// s  sort_vec init 
	vector<int> id_s_sort;
	for (int id_s = 0; id_s < N_s; id_s++) {
		if (c_connected_to_s[id_s].size() > 0) {
			id_s_sort.emplace_back(id_s);
		}
	}
	//t_sort init
	vector<int> t_sort;
	for (int t = 0; t < N_t; t++) {
		t_sort.emplace_back(t);
	}
	//free_num init 95%
	vector<int> free_num(N_s, N_t / 20);



	//sort for burst
	random_shuffle(id_s_sort.begin(), id_s_sort.end());
	sort(id_s_sort.begin(), id_s_sort.end(), [&](const int &a, const int &b){
		return site_bandwidth_vec[a] > site_bandwidth_vec[b];
	});
	//burst
	//find 90% id_s
	vector<int> find10_id_s_vec;
	for (int i = 0; i < 10; i++) {
		int &id_s = id_s_sort[i];
		free_num[id_s] = N_t / 10;
		find10_id_s_vec.emplace_back(id_s);
	}	
	//burst
	for (int &id_s : id_s_sort) {
		priority_queue <tuple<double, int>, vector<tuple<double, int>>, greater<tuple<double, int>>> burst_queue;//升序队列，小顶堆
		for (int t = 0; t < N_t; t++) {
			Demand_t &demand_t = demand_t_vec[t];
			double demand_t_c_sum = 0;
			for (int &id_c : c_connected_to_s[id_s]) {
				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.destination[stream_id][id_c] == -1) {
						demand_t_c_sum += (double)demand_t.demand[stream_id][id_c] / s_connected_to_c[id_c].size();
					}
				}
			}
			if (demand_t_c_sum > base_cost) {
				if (burst_queue.size() < (free_num[id_s])) {
					burst_queue.push(tuple<double, int>(demand_t_c_sum, t));
				}
				else if (demand_t_c_sum > get<0>(burst_queue.top())) {
					burst_queue.pop();
					burst_queue.push(tuple<double, int>(demand_t_c_sum, t));
				}
			}
		}
		while (!burst_queue.empty()) {
			int t = get<1>(burst_queue.top());
			burst_queue.pop();
			Demand_t &demand_t = demand_t_vec[t];
			vector<tuple<int, int, int>> demand_vec; 
			for (int &id_c : c_connected_to_s[id_s]) {
				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.destination[stream_id][id_c] == -1) {
						demand_vec.emplace_back(tuple<int, int, int>(demand_t.demand[stream_id][id_c], stream_id, id_c));
					}
				}
			}
			sort(demand_vec.begin(), demand_vec.end(), greater<tuple<int, int, int>>());//降序
			for (int i = 0; i < demand_vec.size(); i++) {
				int band = get<0>(demand_vec[i]);
				int stream_id = get<1>(demand_vec[i]);
				int id_c = get<2>(demand_vec[i]);
				if (band <= free_band[t][id_s]) {
					demand_t.destination[stream_id][id_c] = id_s;
					free_band[t][id_s] -= band;
					used_band[t][id_s] += band;
					if (free_band[t][id_s] == 0) {
						break;
					}
				}
			}
		}
	}

	//sort for connect
	for (int id_c = 0; id_c < N_c; id_c++) {
		sort(s_connected_to_c[id_c].begin(), s_connected_to_c[id_c].end(), [&](const int &a, const int &b){
			return site_bandwidth_vec[a] > site_bandwidth_vec[b];
		});
	}
	//connect

	vector<double> max_band_costed(N_s, 0);
	for (int &t : t_sort) {
		Demand_t &demand_t = demand_t_vec[t];//arrange one day demand
		priority_queue <tuple<int, int, int>, vector<tuple<int, int, int>>, less<tuple<int, int, int>>> d_queue;//降序队列，大顶堆 
		for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
			for (int id_c = 0; id_c < N_c; id_c++) {
				if (demand_t.destination[stream_id][id_c] == -1) {
					d_queue.push(tuple<int, int, int>(demand_t.demand[stream_id][id_c], stream_id, id_c));
				}
			}
		}
		while (!d_queue.empty()) {
			int _demand = get<0>(d_queue.top());
			int stream_id = get<1>(d_queue.top());
			int id_c = get<2>(d_queue.top());
			d_queue.pop();

			int tar_id_s = -1;
			double d_cost_min = DBL_MAX;

			for (int &id_s : s_connected_to_c[id_c]) {
				if (free_band[t][id_s] >= _demand) {

					int used = used_band[t][id_s] + _demand;
					if (used <= max_band_costed[id_s] || used <= base_cost) {//不影响分数
						tar_id_s = id_s;
						break;
					}

					// 影响分数
					int origin_cost = cul_cost(max_band_costed[id_s], site_bandwidth_vec[id_s]);
					int new_cost = cul_cost(used, site_bandwidth_vec[id_s]);
					double delta_cost = new_cost - origin_cost;
					if (d_cost_min > delta_cost) {
						d_cost_min = delta_cost;
						tar_id_s = id_s;
					}
				}
			}

			if (tar_id_s == -1) {
				cout<<"err0"<<endl;
				exit(0);
			}

			demand_t.destination[stream_id][id_c] = tar_id_s;
			used_band[t][tar_id_s] += _demand;
			free_band[t][tar_id_s] -= _demand;

			if (used_band[t][tar_id_s] > max_band_costed[tar_id_s]) {
				max_band_costed[tar_id_s] = used_band[t][tar_id_s];
			}
		}
	}

	//migrate initial
	vector<Usedband_t_set> used_t_set(N_s);
	for (int &id_s : id_s_sort) {

		used_t_set[id_s].big_size = free_num[id_s];// N_t/20
		used_t_set[id_s].small_size = N_t - used_t_set[id_s].big_size;// N_t - N_t/20

		sort(t_sort.begin(), t_sort.end(), [&](const int &a, const int &b){
			return used_band[a][id_s] < used_band[b][id_s];
		});

		for (int t_i = 0; t_i < used_t_set[id_s].small_size; t_i++) {
			int &t = t_sort[t_i];
			used_t_set[id_s].small_set.emplace(used_band[t][id_s], t);
		}

		for (int t_i = N_t - 1; t_i >= used_t_set[id_s].small_size; t_i--) {
			int &t = t_sort[t_i];
			used_t_set[id_s].big_set.emplace(used_band[t][id_s], t);
		}
	}
	//print cost
	int now_cost = calculate_cost(used_t_set);
	cout<<0<<","<<now_cost<<endl;
	//sort for migrate
	random_shuffle(id_s_sort.begin(), id_s_sort.end());
	sort(id_s_sort.begin(), id_s_sort.end(), [&](const int &a, const int &b){
		/*if (site_bandwidth_vec[a] == site_bandwidth_vec[b]) {
			c_connected_to_s[a].size() < c_connected_to_s[b].size();
		}*/
		return site_bandwidth_vec[a] < site_bandwidth_vec[b];
	});
	/*for (int id_c = 0; id_c < N_c; id_c++) {
		sort(s_connected_to_c[id_c].begin(), s_connected_to_c[id_c].end(), [&](const int &a, const int &b){
			return site_bandwidth_vec[a] > site_bandwidth_vec[b];
		});
	}*/
	//migrate type 1 & 2
	auto migrate_type_1_2 = [&](){

		//bad id_s for migrate type2
		vector<tuple<int, int>> bad_s_t_vec;
		vector<vector<bool>> s_t_visited(N_s, vector<bool>(N_t, false));

		//migrate type1
		for (int &id_s : id_s_sort) {

			bool migrate_flag = true;
			while (migrate_flag && used_t_set[id_s].get_score_band() > base_cost) {
				
				vector<tuple<int, int>> bad_s_t_vec_temp;

				vector<int> migrate_t_vec;// t need migrate
				used_t_set[id_s].get_migrate_small_t_vec(migrate_t_vec);

				//migrate each t
				for (int &t : migrate_t_vec) {
					Demand_t &demand_t = demand_t_vec[t];
					tuple<int, int, int, int> best_flow_c_stream_tos(-1, -1, -1, -1);//flow max
	
					for (int &id_c : c_connected_to_s[id_s]) {
						vector<tuple<int, int, int>> stream_vec;
						for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {//find all stream
							if (demand_t.destination[stream_id][id_c] == id_s) {
								stream_vec.emplace_back(tuple<int, int, int>(demand_t.demand[stream_id][id_c], rand(), stream_id));
							}
						}
						if (stream_vec.empty()) {
							continue;
						}
						int max_stream_pos = stream_vec.size() - 1;
						sort(stream_vec.begin(), stream_vec.end(), greater<tuple<int, int, int>>());
						
						for (int &to_id_s : s_connected_to_c[id_c]) {
							if (id_s == to_id_s || used_t_set[to_id_s].is_free()) {
								continue;
							}

							int score_band = used_t_set[to_id_s].get_score_band();
							int max_band_can_load = 0;
							int s_type = used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t));
							/*if (used_band[t][to_id_s] <= base_cost) {
								max_band_can_load = min(base_cost, site_bandwidth_vec[to_id_s])- used_band[t][to_id_s];
							}
							else {
								int s_type = used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t));
								if (s_type == 1) {
									max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
								}
								else if (s_type == 2) {
									max_band_can_load = 0;
								}
								else if (s_type == 3){
									max_band_can_load = score_band - used_band[t][to_id_s];
								}
							}*/
							if (s_type == 1) {
								max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
							}
							else if (s_type == 2) {
								max_band_can_load = 0;
							}
							else if (s_type == 3){
								max_band_can_load = score_band - used_band[t][to_id_s];
							}


							if (max_band_can_load <= 0) {
								bad_s_t_vec_temp.emplace_back(to_id_s, t);
								continue;
							}

							for (int pos = 0; pos <= max_stream_pos; pos++) {
								auto &stream = stream_vec[pos];
								int stream_flow = get<0>(stream);
								int stream_id = get<2>(stream);
								if (stream_flow <= max_band_can_load) {
									
									if ( get<0>(best_flow_c_stream_tos) < stream_flow) {
										best_flow_c_stream_tos = tuple<int, int, int, int> (stream_flow, id_c, stream_id, to_id_s);
										max_stream_pos = pos;
									}
									break;
								}
							}

							if (get<3>(best_flow_c_stream_tos) != -1) {
								goto OPERATION;
							}
							else {
								bad_s_t_vec_temp.emplace_back(to_id_s, t);
							}

							if (max_stream_pos == 0) {
								break;
							}
						}
					}

					OPERATION:
					//operation
					if (get<3>(best_flow_c_stream_tos) != -1) {
						int id_c = get<1>(best_flow_c_stream_tos);
						int stream_id = get<2>(best_flow_c_stream_tos);
						int to_id_s = get<3>(best_flow_c_stream_tos);

						move(demand_t, used_t_set, free_band, used_band, t, stream_id, id_c, id_s, to_id_s);
					}
					else {//bad id_s
						migrate_flag = false;//don't need migrate this id_s all t
						for (auto &bad_s_t : bad_s_t_vec_temp) {
							bad_s_t_vec.emplace_back(bad_s_t);
							s_t_visited[get<0>(bad_s_t)][get<1>(bad_s_t)] = true;
						}

					}
				}
			}
		}

		//migrate type2
		sort(bad_s_t_vec.begin(), bad_s_t_vec.end());
		bad_s_t_vec.erase(unique(bad_s_t_vec.begin(), bad_s_t_vec.end()), bad_s_t_vec.end());

		for (auto &bad_s_t : bad_s_t_vec) {
			int id_s = get<0>(bad_s_t);
			int t = get<1>(bad_s_t);

			Demand_t &demand_t = demand_t_vec[t];

			for (int &id_c : c_connected_to_s[id_s]) {
				for (int &to_id_s : s_connected_to_c[id_c]) {
					if (id_s == to_id_s || used_t_set[to_id_s].is_free()) {
						continue;
					}

					if (s_t_visited[to_id_s][t] == true) {
						continue;
					}

					//migrate max flow
					int score_band = used_t_set[to_id_s].get_score_band();
					int max_band_can_load = 0;
					int s_type = used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t));
					/*if (s_type == 1) {
						max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
					}
					else if (s_type == 2) {
						max_band_can_load = 0;
					}
					else if (s_type == 3){
						max_band_can_load = score_band - used_band[t][to_id_s];
					}*/
					if (s_type == 1) {
						max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
					}
					else if (s_type == 2) {
						max_band_can_load = max(score_band, min(base_cost, site_bandwidth_vec[to_id_s])) - used_band[t][to_id_s];
					}
					else if (s_type == 3){
						max_band_can_load = max(score_band, min(base_cost, site_bandwidth_vec[to_id_s])) - used_band[t][to_id_s];
					}

					if (max_band_can_load <= 0) {
						continue;
					}

					for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {//find all stream
						if (demand_t.destination[stream_id][id_c] == id_s) {
							int _demand = demand_t.demand[stream_id][id_c];
							if (max_band_can_load >= _demand) {
								move(demand_t, used_t_set, free_band, used_band, t, stream_id, id_c, id_s, to_id_s);
								max_band_can_load -= _demand;
							}
						}
					}
					
				}
			}
		}

	};
	//migrate type3
	auto migrate_type_3 = [&](){
		for (int &id_s : id_s_sort) {
			vector<int> migrate_t_vec;// t need migrate
			used_t_set[id_s].get_migrate_big_t_vec(migrate_t_vec);
			if (migrate_t_vec.size() == 0) {
				continue;
			}
			//migrate each t
			for (int &t : migrate_t_vec) {
				Demand_t &demand_t = demand_t_vec[t];
				//can move all ?
				int sum_band_can_load = 0;
				for (int &id_c : c_connected_to_s[id_s]) {
					for (int &to_id_s : s_connected_to_c[id_c]) {
						if (id_s == to_id_s || used_t_set[to_id_s].is_free()) {
							continue;
						}

						int score_band = used_t_set[to_id_s].get_score_band();
						int max_band_can_load = 0;

						if (used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t)) == 3){
								max_band_can_load = score_band - used_band[t][to_id_s];
						}
						/*if (used_band[t][to_id_s] <= base_cost) {
							max_band_can_load = min(base_cost, site_bandwidth_vec[to_id_s])- used_band[t][to_id_s];
						}
						else {
							int s_type = used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t));
							if (s_type == 1) {
								max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
							}
							else if (s_type == 2) {
								continue;
							}
							else if (s_type == 3){
								max_band_can_load = score_band - used_band[t][to_id_s];
							}
						}*/
						sum_band_can_load += max_band_can_load;
					}
				}
				if (sum_band_can_load < used_band[t][id_s] - used_t_set[id_s].get_score_band()) {
					continue;
				}
				//move all
				for (int &id_c : c_connected_to_s[id_s]) {
					for (int &to_id_s : s_connected_to_c[id_c]) {
						if (id_s == to_id_s || used_t_set[to_id_s].is_free()) {
							continue;
						}

						int score_band = used_t_set[to_id_s].get_score_band();
						int max_band_can_load = 0;


						if (used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t)) == 3){
								max_band_can_load = score_band - used_band[t][to_id_s];
						}
						/*if (used_band[t][to_id_s] <= base_cost) {
							max_band_can_load = min(base_cost, site_bandwidth_vec[to_id_s])- used_band[t][to_id_s];
						}
						else {
							int s_type = used_t_set[to_id_s].get_state(pair<int, int>(used_band[t][to_id_s], t));
							if (s_type == 1) {
								max_band_can_load = site_bandwidth_vec[to_id_s] - used_band[t][to_id_s];
							}
							else if (s_type == 2) {
								continue;
							}
							else if (s_type == 3){
								max_band_can_load = score_band - used_band[t][to_id_s];
							}
						}*/
						if (max_band_can_load == 0) {
							continue;
						}

						for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {//find all stream
							if (demand_t.destination[stream_id][id_c] == id_s) {
								int _demand = demand_t.demand[stream_id][id_c];
								if (max_band_can_load >= _demand) {
									move(demand_t, used_t_set, free_band, used_band, t, stream_id, id_c, id_s, to_id_s);
									max_band_can_load -= _demand;
								}
							}
						}

					}
				}

			}
		}
	};


	/*if (used_t_set[id_s].get_score_band() > score_start) {
		//vector<tuple<int, int, int, int, int>> operation_list;
		//operation_list.emplace_back(tuple<int, int, int, int, int>(t, stream_id, id_c, id_s, to_id_s));
		for (auto operation : operation_list) {
			int t = get<0>(operation);
			int stream_id = get<1>(operation);
			int id_c = get<2>(operation);
			int id_s = get<3>(operation);
			int to_id_s = get<4>(operation);
			move(demand_t_vec[t], used_t_set, free_band, used_band, t, stream_id, id_c, to_id_s, id_s);
		}
	}*/

	t_once_max = fmax(t_once_max, time(NULL) - t_once_start);


	int same_cost_cnt = 0;
	int last_cost = -1;
	int cnt = 1;
	while(time(NULL) - start_time < MAX_TIME) {

		migrate_type_1_2();

		if (cnt % 10 == 0) {//&& rand() < 0.1 * RAND_MAX) { //(
			migrate_type_3();
		}
		

		last_cost = now_cost;
		now_cost = calculate_cost(used_t_set);
		if (last_cost == now_cost) {
			same_cost_cnt++;
			if (same_cost_cnt > cnt / 3 && same_cost_cnt >= 4) {
				break;
			}
		}
		else {
			same_cost_cnt = 0;
		}

		cout<<cnt++<<","<<now_cost<<endl;
	}




	if (min_cost > now_cost) {//save best ans
		min_cost = now_cost;
		//save best state
		for (int t = 0; t < N_t; t++) {
			Demand_t &demand_t = demand_t_vec[t];
			demand_t.ans = demand_t.destination;
		}
		//save best state
	}


	if (time(NULL) - start_time + t_once_max < MAX_TIME) {
		goto START;
	}


	cout<<"time = "<<(time(NULL) - start_time)<<" s"<<endl;

OUTPUT:

	{//output solution.txt
		file_output.open(output_path);

		for (int i = 0; i < 10; i++) {
			int &id_s_10 = find10_id_s_vec[i];
			if (i == 9) {
				file_output << id2n_s[id_s_10] << endl;
			}
			else {
				file_output << id2n_s[id_s_10] << ",";
			}
		}


		for (int t = 0; t < N_t; t++) {
			Demand_t &demand_t = demand_t_vec[t];
			for (int id_c = 0; id_c < N_c; id_c++) {
				file_output << id2n_c[id_c] << ":";
		
				bool flag = true;
				vector<vector<int>> rx_stream_id_vec(N_s);//id_s to rx_stream_id_vec

				for (int stream_id = 0; stream_id < demand_t.stream_num; stream_id++) {
					if (demand_t.ans[stream_id][id_c] != -1) {
						rx_stream_id_vec[demand_t.ans[stream_id][id_c]].emplace_back(stream_id);
					}
				}

				for (int id_s = 0; id_s < N_s; id_s++) {
					if (rx_stream_id_vec[id_s].size() > 0) {
						if (flag) {
							flag = false;
						}
						else {
							file_output << "," ;
						}
						file_output << "<" << id2n_s[id_s];
						for (int &rx_stream_id : rx_stream_id_vec[id_s]) {
							file_output << "," << demand_t.stream_name[rx_stream_id];
						}
						file_output << ">";
					}
				}

				file_output << endl;
			}
		}
		file_output.close();
	}

	return 0;
}
