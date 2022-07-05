#ifndef FUNCTION_H
#define FUNCTION_H
#include <bits/stdc++.h>
using namespace std;

const int N = 140, M = 40;
//const string file_demand = "../MyWorkC++/CodeCraft/benchmark/demand.csv";
const string file_demand = "../MyWorkC++/CodeCraft2/benchmark/data/demand.csv";
const string file_bandWidth = "../MyWorkC++/CodeCraft2/benchmark/data/site_bandwidth.csv";
const string file_qos = "../MyWorkC++/CodeCraft2/benchmark/data/qos.csv";
const string file_qosConfig = "../MyWorkC++/CodeCraft2/benchmark/data/config.ini";
const string file_output = "../MyWorkC++/CodeCraft2/benchmark/output/solution.txt";

typedef struct EdgeNode{
    string name;
    int bandwidth;
    bool visited;
}EdgeNode, *EdgeNodes;


typedef struct CustomNode{
    string name;
    set<string> qos;
}CustomNode, *CustomNodes;

typedef struct Answer{
    map<string, int> mp;
}Answer, *Answers;


class solution
{
    private:
        static solution* instance;

        int maxQos;
        int T, realN, realM, groupCnt;
        int h[N*M], w[N*M], f[N*M], e[N*M], ne[N*M], idx;
        int dist[2*N+M], pre[2*N+M], st[2*N+M];
        bool beginFlag;
        ifstream fin;
        ofstream fout;
        EdgeNodes edgeNodes;
        CustomNodes customNodes;
        deque<pair<CustomNode, int>> demandInfos[8935];
        Answers answers;
        map<string, int> edgeOfId;
        map<string, int> customOfId;
        map<int, int> customVarId;
        map<int, int> edgeVarId;
        map<int, pair<int, int>> freeBandwidth;

    public:
        solution();
        virtual ~solution();
        static solution* getInstance();

        void init_nodes();
        void add(int a, int b, int c, int d);
        static bool vecCmp(const string &a, const string &b);
        bool spfa(int s, int t);
        void slove();
        deque<pair<CustomNode, int>> get_next_demand();
        void handle(deque<pair<CustomNode, int>>& demandInfo);
        int maxflow(int s, int t);
        void BFrag();
        void printf_result(bool beginFlag);
};
#endif // FUNCTION_H
