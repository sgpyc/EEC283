#include <map>
#include <list>
#include <omp.h>

using namespace std;

template <
    typename IdType,
    typename ValueType,
    typename SizeType>
class InputForest
{
typedef map<ValueType, IdType> MapT;
typedef typename MapT::iterator MapTI;
typedef pair<ValueType, IdType> MapTP;
typedef list<IdType> ListT;

static const SizeType MAX_NODEID = 1048576;
static const SizeType PAGE_SIZE = 1024; 
static const SizeType MAX_PAGE = MAX_NODEID / PAGE_SIZE + 1;
static const ValueType NULL_Value = -1;
static const IdType NULL_Id = -1;

public:

    MapT **maps;
    ListT *empty_nodes;
    int num_threads;
    SizeType num_inputs;
    SizeType page_counter;
    SizeType *input_widths;
    int **markers;
    //SizeType MAX_NODEID;// = 1048576;
    //SizeType PAGE_SIZE;// = 1024;
    //SizeType MAX_PAGE;// = MAX_NODEID / PAGE_SIZE + 1;
   
    InputForest() :
        maps         (NULL),
        empty_nodes  (NULL),
        num_threads  (0),
        num_inputs   (0),
        page_counter (0),
        input_widths (NULL),
        markers      (NULL)
        //MAX_NODEID   (1048576),
        //PAGE_SIZE    (1024),
        //MAX_PAGE     (MAX_NODEID / PAGE_SIZE +1)
    {
    }

    ~InputForest()
    {
        Release();
    }

    int Release()
    {
        int retval = 0;
        
        for (SizeType i=0; i<page_counter; i++)
        {
            maps[i]->clear();
            delete[] maps[i];maps[i] = NULL;
        }
        delete[] maps; maps = NULL;
        delete[] empty_nodes; empty_nodes = NULL;
        delete[] input_widths; input_widths = NULL;
        for (int i=0; i<num_threads; i++)
        {
            delete[] markers[i]; markers[i] = NULL;
        }
        delete[] markers; markers=NULL;
        page_counter = 0;
        return retval;
    }

    int Init(int num_threads, SizeType num_inputs, SizeType *input_widths)
    {
        int retval = 0;
        retval = Release();
        if (retval != 0) return retval;
        this->num_threads = num_threads;

        maps = new MapT*[MAX_PAGE];
        for (SizeType i=0; i<MAX_PAGE; i++)
            maps[i] = NULL;
        empty_nodes = new ListT[num_threads];
        for (int i=0; i<num_threads; i++)
        {
            maps[i] = new MapT[PAGE_SIZE];
            for (IdType map_id = i*PAGE_SIZE; map_id < (i+1) * PAGE_SIZE; map_id++)
            {
                empty_nodes[i].push_front(map_id);
            }
        }
        page_counter = num_threads;

        this->input_widths = new SizeType[num_inputs];
        memcpy(this->input_widths, input_widths, sizeof(SizeType) * num_inputs);
        SizeType max_widths = 0;
        for (SizeType i=0; i<num_inputs; i++)
            if (input_widths[i] > max_widths) max_widths= input_widths[i];
        markers = new int*[num_threads];
        for (int i=0; i<num_threads; i++)
        {
            markers[i] = new int[1<<max_widths];
            memset(markers[i], 0, sizeof(int) * (1<<max_widths));
        }
        return retval;
    }

    int NewNode(int thread_num, IdType &node_id, MapT* &map)
    {
        int retval = 0;
        if (thread_num > num_threads)
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: "
                <<" thread_num "<<thread_num<<" > "<<num_threads<<endl;
            return 1;
        }

        if (empty_nodes[thread_num].empty())
        {
            SizeType counter = 0;
            #pragma omp atomic capture
            //{
                counter = page_counter++;
                //page_counter ++;
            //}
            maps[counter] = new MapT[PAGE_SIZE];
            for (IdType map_id = counter*PAGE_SIZE; map_id < (counter+1) * PAGE_SIZE; map_id++)
            {
                empty_nodes[thread_num].push_front(map_id);
            }
        }
        node_id = empty_nodes[thread_num].back();
        empty_nodes[thread_num].pop_back();
        map = GetMap(node_id);

        return retval;
    }

    MapT* GetMap(IdType node_Id)
    {
        return &maps[node_Id / PAGE_SIZE][node_Id % PAGE_SIZE];
    }

    int ReleaseNode(int thread_num, IdType node_Id)
    {
        int retval = 0;
        if (node_Id >= page_counter * PAGE_SIZE) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: "
                <<" node_id "<<node_Id<<" >= "<<page_counter * PAGE_SIZE<<endl;
        }
        GetMap(node_Id)->clear();
        empty_nodes[thread_num].push_front(node_Id);
        return retval;
    }

    int NewTree(int thread_num, SizeType input_num, ValueType input_value, IdType &root_Id)
    {
        MapT* current_map = NULL, *next_map = NULL;
        IdType node_Id = NULL_Id;
        int retval = 0;

        for (SizeType i=0; i<num_inputs; i++)
        {
            retval = NewNode(thread_num, node_Id, next_map);
            if (retval !=0) return retval;
            if (i==0)
            {
                root_Id = node_Id;
            } else {
                current_map->insert(MapTP(
                    (i-1 == input_num) ? input_value : NULL_Value, node_Id));
            }
            if (i == num_inputs-1)
            {
                next_map->insert(MapTP(
                    (i == input_num) ? input_value : NULL_Value, NULL_Id));
            }
            current_map = next_map;
        }
        return retval;
    }

    int ReleaseTree(int thread_num, IdType root_Id)
    {
        MapT* map=GetMap(root_Id);
        int retval = 0;
        for (MapTI it = map->begin(); it != map->end(); it++)
        {
            if (it->second != NULL_Id) retval = ReleaseTree(thread_num, it->second);
            if (retval != 0) return retval;
        }
        retval = ReleaseNode(thread_num, root_Id);
        return retval;
    }

    int CopyTree(int thread_num, IdType org_root, IdType &new_root)
    {
        MapT* new_map = NULL;
        MapT* org_map = GetMap(org_root);
        int retval = 0;
        retval = NewNode(thread_num, new_root, new_map);
        if (retval != 0) return retval;
        for (MapTI it = org_map->begin(); it != org_map->end(); it++)
        {
            if (it->second == NULL_Id)
            {
                new_map->insert(MapTP(it->first, NULL_Id));
            } else {
                IdType new_child = NULL_Id;
                retval = CopyTree(thread_num, it->second, new_child);
                if (retval != 0) return retval;
                new_map->insert(MapTP(it->first, new_child));
            }
        }
        return retval; 
    }

    int AddTree(int thread_num, IdType root_A, IdType root_B, IdType& root_C, SizeType level)
    {
        MapT* map_A = GetMap(root_A);
        MapT* map_B = GetMap(root_B);
        MapT* map_C = NULL;
        ValueType value_A = NULL_Value, value_B = NULL_Value;//, value_C = NULL_Value;
        IdType Id_A = NULL_Id, Id_B = NULL_Id, Id_C = NULL_Id;
        MapTI it_A, it_B;
        int retval = 0;
        bool has_A = false;

        retval = NewNode(thread_num, root_C, map_C);
        if (retval != 0) return retval;

        for (it_A = map_A->begin(); it_A != map_A->end(); it_A++)
        {
            value_A = it_A->first;
            Id_A = it_A->second;

            if (value_A == NULL_Value)
            { // *X* + *.*
                if (Id_A == NULL_Id)
                {  // *X + *.
                    map_C->insert(MapTP(NULL_Value, NULL_Id));
                } else { // *X... + *....
                    bool has_B = false;
                    for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
                    {
                        value_B = it_B->first;
                        Id_B = it_B->second;
                        if (value_B == NULL_Value)
                        { // *X... + *X...
                            retval = AddTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            map_C->insert(MapTP(NULL_Value, Id_C));
                            break;
                        } else { // *X... + *B...
                            retval = CopyTree(thread_num, Id_B, Id_C);
                            if (retval != 0) return retval;
                            map_C->insert(MapTP(value_B, Id_C));
                            if (!has_B)
                            {
                                memset(markers[thread_num], 0, sizeof(int) * (1<<input_widths[level]));
                                has_B = true;
                            }
                            markers[thread_num][value_B] = 1; 
                        }
                    }
                    if (has_B)
                    { // *!B... 
                        for (value_A = 0; value_A < (1<<input_widths[level]); value_A++)
                        {
                            if (markers[thread_num][value_A] == 1) continue;
                            retval = CopyTree(thread_num, Id_A, Id_C);
                            if (retval != 0) return retval;
                            map_C->insert(MapTP(value_A, Id_C));
                        }
                    }
                }
                break;
            } else { // *A* + *.*
                has_A = true;
                if (Id_A == NULL_Id)
                { // *A + *.
                    map_C->insert(MapTP(value_A, NULL_Id));
                } else { // *A... + *....
                    it_B = map_B->find(value_A);
                    if (it_B == map_B->end())
                    { // *A... + *!A...
                        retval = CopyTree(thread_num, Id_A, Id_C);
                        if (retval != 0) return retval;
                        map_C->insert(MapTP(value_A, Id_C));
                    } else { // *A... + *A...
                        retval = AddTree(thread_num, Id_A, it_B->second, Id_C, level+1);
                        if (retval !=0) return retval;
                        map_C->insert(MapTP(value_A, Id_C));
                    }
                }
            }
        }

        if (has_A)
        { // *A* + *.*
            for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
            {
                value_B = it_B->first;
                Id_B = it_B->second;
                if (value_B == NULL_Value)
                { // *A* + *X*
                    if (Id_B == NULL_Id)
                    { // *A + *X
                        retval = ReleaseTree(thread_num, Id_C);
                        if (retval != 0) return retval;
                        retval = NewNode(thread_num, root_C, map_C);
                        map_C->insert(MapTP(NULL_Value, NULL_Id));
                    } else { // *A... + *X...
                        memset(markers[thread_num], 0, sizeof(int) * (1<<input_widths[level]));
                        for (it_A = map_A->begin(); it_A != map_A->end(); it_A++)
                            markers[thread_num][it_A->first] = 1;
                        for (value_B = 0; value_B < (1<<input_widths[level]); value_B++)
                        { // *!A...
                            if (markers[thread_num][value_B] == 1) continue;
                            retval = CopyTree(thread_num, Id_B, Id_C);
                            if (retval != 0) return retval;
                            map_C->insert(MapTP(value_B, Id_C));
                        }
                    }
                    break;
                } else { // *A* + *B*
                    it_A = map_A->find(value_B);
                    if (it_A == map_A->end())
                    { // *!B* + *B*
                        if (Id_B != NULL_Id)
                        { // *!B... + *B...
                            retval = CopyTree(thread_num, Id_B, Id_C);
                            if (retval !=0) return retval;
                            map_C->insert(MapTP(value_B, Id_C));
                        } else { // *!B + *B
                            map_C->insert(MapTP(value_B, NULL_Id));
                        }
                    }
                }
            }
        }
        return retval;
    }

    int SubTree(int thread_num, IdType root_A, IdType root_B, IdType& root_C, SizeType level)
    {
        MapT* map_A = GetMap(root_A);
        MapT* map_B = GetMap(root_B);
        MapT* map_C = NULL;
        ValueType value_A = NULL_Value, value_B = NULL_Value;//, value_C = NULL_Value;
        IdType Id_A = NULL_Id, Id_B = NULL_Id, Id_C = NULL_Id;
        MapTI it_A, it_B;
        int retval = 0;
        
        retval = NewNode(thread_num, root_C, map_C);
        if (retval != 0) return retval;

        for (it_A = map_A->begin(); it_A != map_A->end(); it_A++)
        {
            value_A = it_A->first;
            Id_A = it_A->second;

            if (value_A == NULL_Value)
            { // *X* - *.*
                if (Id_A == NULL_Id)
                {  // *X - *.
                    bool has_B = false;
                    for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
                    {
                        value_B = it_B->first;
                        Id_B = it_B->second;
                        if (value_B == NULL_Value)
                        { // *X - *X
                            retval = ReleaseTree(thread_num, root_C);
                            if (retval != 0) return retval;
                            root_C = NULL_Id; return retval;
                            break;
                        } else { // *X - *B
                            if (!has_B)
                            {
                                memset(markers[thread_num], 0, sizeof(int) * (1<<input_widths[level]));
                                has_B = true;
                            }
                            markers[thread_num][value_B] = 1;
                        }
                    }
                    if (has_B)
                    {  // *!B
                        for (value_A = 0; value_A < (1<<input_widths[level]); value_A++)
                        {
                            if (markers[thread_num][value_A] == 1) continue;
                            map_C->insert(MapTP(value_A, NULL_Id));
                        }
                    }
                    break;
                } else { //*X... - *....
                    bool has_B = false;
                    for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
                    {
                        value_B = it_B->first;
                        Id_B = it_B->second;
                        if (value_B == NULL_Value)
                        { // *X... - *X...
                            retval = SubTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            if (Id_C != NULL_Id) map_C->insert(MapTP(NULL_Value, Id_C));
                            break;
                        } else { // *X... - *B...
                            if (!has_B)
                            {
                                memset(markers[thread_num], 0, sizeof(int) * (1<<input_widths[level]));
                                has_B = true;
                            }
                            markers[thread_num][value_B] = 1;
                            retval = SubTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            if (Id_C != NULL_Id) map_C->insert(MapTP(value_B, Id_C));
                        }
                    }
                    if (has_B) 
                    { // *!B...
                        for (value_A = 0; value_A < (1<<input_widths[level]); value_A++)
                        {
                            if (markers[thread_num][value_A] == 1) continue;
                            retval = CopyTree(thread_num, Id_A, Id_C);
                            if (retval != 0) return retval;
                            map_C->insert(MapTP(value_A, Id_C));
                        }
                    }
                }
                break;
            } else { // *A* - *.*
                if (Id_A == NULL_Id)
                { // *A - *.
                    it_B = map_B->find(value_A);
                    if (it_B == map_B->end())
                    {
                        if (map_B->begin()->first == NULL_Value)
                        { // *A - *X
                        } else {
                            map_C->insert(MapTP(value_A, NULL_Id));
                        }
                    } else { // *A-*A
                    }
                } else { //*A... - *....
                    if (map_B->begin()->first == NULL_Value)
                    { // *A... - *X...
                        retval = SubTree(thread_num, Id_A, Id_B, Id_C, level+1);
                        if (retval != 0) return retval;
                        if (Id_C!=NULL_Id) map_C->insert(MapTP(value_A, Id_C));
                    } else {
                        it_B = map_B->find(value_A);
                        if (it_B != map_B->end())
                        { // *A... - *A...
                            retval = SubTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            if (Id_C!=NULL_Id) map_C->insert(MapTP(value_A, Id_C));
                        }
                    }
                }
            }
        }

        if (map_C->size() == 0)
        {
            retval = ReleaseTree(thread_num, root_C);
            if (retval != 0) return retval;
            root_C = NULL_Id;
        }
        return retval;
    }

    int Is_EqualTree(IdType root_A, IdType root_B, bool& is_equal)
    {
        MapT* map_A = GetMap(root_A);
        MapT* map_B = GetMap(root_B);
        ValueType value_A = NULL_Value, value_B = NULL_Value;
        IdType Id_A = NULL_Id, Id_B = NULL_Id;
        int retval = 0;
        MapTI it_A, it_B;

        for (it_A = map_A->begin(); it_A != map_A->end(); it_A++)
        {
            value_A = it_A->first;
            Id_A = it_A->second;
            it_B = map_B->find(value_A);
            if (it_B == MapT::end())
            {
                is_equal = false;
                return 0;
            }

            value_B = it_B->first;
            Id_B = it_B->second;
            if (Id_A == NULL_Id)
            { // *A, *A*
                if (Id_B == NULL_Id)
                { // *A, *A
                    continue;
                } else { // *A, *A...
                    is_equal = false;
                    return 0;
                }
            } else if (Id_B == NULL_Id)
            {  // *A..., *A
                is_equal = false;
                return 0;
            } else { // *A..., *A...
                retval = Is_EqualTree(Id_A, Id_B, is_equal);
                if (retval != 0) return retval;
                if (!is_equal) return 0;
                continue;
            }
        }

        for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
        {
            value_B = it_B->first;
            Id_B = it_B->second;
            it_A = map_A->find(value_A);
            if (it_A == MapT::End())
            {
                is_equal = false;
                return 0;
            }
        }
        is_equal = true;
        return retval;
    }

    int AndTree(int thread_num, IdType root_A, IdType root_B, IdType& root_C, SizeType level)
    {
        MapT* map_A = GetMap(root_A);
        MapT* map_B = GetMap(root_B);
        MapT* map_C = GetMap(root_C);
        ValueType value_A = NULL_Value, value_B = NULL_Value;//, value_C = NULL_Value;
        IdType Id_A = NULL_Id, Id_B = NULL_Id, Id_C = NULL_Id;
        MapTI it_A, it_B;
        int retval = 0;

        retval = NewNode(thread_num, root_C, map_C);
        if (retval !=0) return retval;

        for (it_A = map_A->begin(); it_A != map_A->end(); it_A++)
        {
            value_A = it_A->first;
            Id_A = it_A->second;

            if (value_A == NULL_Value)
            { // *X* & *.*
                //bool has_B = false;
                for (it_B = map_B->begin(); it_B != map_B->end(); it_B++)
                {
                    value_B = it_B->first;
                    Id_B = it_B->second;
                    if (value_B == NULL_Value)
                    { // *X* & *X*
                        if (Id_A == NULL_Id && Id_B == NULL_Id)
                        {  // *X & *X
                            map_C->insert(MapTP(NULL_Value, NULL_Id));
                        } else { // *X... & *X...
                            retval = AndTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval !=0) return retval;
                            if (Id_C != NULL_Id) map_C->insert(MapTP(NULL_Value, Id_C));
                        }
                    } else { // *X* & *B*
                        //has_B = true;
                        if (Id_B == NULL_Id)
                        { // *X & *B
                            map_C->insert(MapTP(value_B, NULL_Id));
                        } else { // *X... & *B...
                            retval = AddTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            if (Id_C != NULL_Id) map_C->insert(MapTP(value_B, Id_C));
                        }
                    }
                }
                break;
            } else { // *A* & *.*
                if (map_B->begin()->first == NULL_Value)
                { // *A* & *X*
                    Id_B = map_B->begin()->second;
                    if (Id_A == NULL_Id)
                    { // *A & *X
                        map_C->insert(MapTP(value_A, NULL_Id));
                    } else { // *A... & *X...
                        retval = AddTree(thread_num, Id_A, Id_B, Id_C, level+1);
                        if (retval !=0) return retval;
                        if (Id_C != NULL_Id) map_C->insert(MapTP(value_A, Id_C));
                    }
                } else { // *A* & *B*
                    it_B = map_B->find(value_A);
                    if (it_B != map_B->end())
                    { // *A* & *A*
                        if (Id_A == NULL_Id)
                        { // *A & *A
                            map_C->insert(MapTP(value_A, NULL_Id));
                        } else { // *A... & *A...
                            retval = AddTree(thread_num, Id_A, Id_B, Id_C, level+1);
                            if (retval != 0) return retval;
                            if (Id_C != NULL_Id) map_C->insert(MapTP(value_A, Id_C));
                        }
                    }
                }
            }
        }

        if (map_C->size() == 0)
        {
            retval = ReleaseTree(thread_num, root_C);
            if (retval != 0) return retval;
            root_C = NULL_Id;
        }
        return retval;
    }
 
};

