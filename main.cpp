#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <cstring>
#include <map>
#include <omp.h>

#include "module.hpp"
#include "net.hpp"
#include "tree.hpp"

using namespace std;

typedef int  SizeType;
typedef int ValueType;
typedef int  IdType;
typedef Module<IdType, ValueType, SizeType> ModuleT;
typedef Net<IdType> NetT;
typedef NetValue<IdType, ValueType, SizeType> NetVT;
typedef InputForest<IdType, ValueType, SizeType> ForestT;
typedef map<ValueType, IdType> MapT;
typedef typename MapT::iterator MapTI;
typedef pair<ValueType, IdType> MapTP;

const IdType NULL_Id = -1;
const SizeType NUM_NET_ERRORS = 7;
const SizeType NUM_MODULE_ERRORS = 3;

SizeType  num_modules = 0;
SizeType  num_nets    = 0;
ModuleT** modules     = NULL;
NetT**    nets        = NULL;
SizeType  num_inputs  = 0;
IdType*   inputs       = NULL;
SizeType* net_widths = NULL;
string*   input_names  = NULL;
SizeType  max_num_inputs = 0;
SizeType  max_num_outputs = 0;
ValueType** thread_inputs = NULL;
ValueType** thread_outputs = NULL;
SizeType  num_errors = 0;
map<IdType, IdType> error_types;
map<IdType, IdType> error_Ids;
map<IdType, IdType> error_refs;
map<IdType, IdType> error_conds;

map<string, IdType> net_map;

int ReadInput(int argc, char* argv[])
{
    ifstream  fin;
    int       retval      = 0;
    pair<map<string, IdType>::iterator, bool> ret;

    if (argc < 2)
    {
        cout<<"Error: no input file name"<<endl;
        return 1;
    }
    fin.open(argv[1]);
    if (!fin.is_open())
    {
        cout<<"Error: cannot open input file "<<argv[1]<<endl;
        return 2;
    }

    fin>>num_modules;
    modules = new ModuleT*[num_modules];
    for (SizeType i=0; i<num_modules; i++)
    {
        string module_type="", str="";
        fin>>module_type;
        transform(module_type.begin(), module_type.end(), 
                  module_type.begin(), ::tolower);
        getline(fin, str);
        //cout<<module_type<<"|"<<str<<endl;
        if      (module_type == "cross") 
            modules[i] = new Connecter<IdType, ValueType, SizeType>();
        else if (module_type == "not"    )
            modules[i] = new NotGate  <IdType, ValueType, SizeType>();
        else if (module_type == "or"     )
            modules[i] = new OrGate   <IdType, ValueType, SizeType>();
        else if (module_type == "xor"    )
            modules[i] = new XorGate  <IdType, ValueType, SizeType>();
        else if (module_type == "nor"    )
            modules[i] = new NorGate  <IdType, ValueType, SizeType>();
        else if (module_type == "and"    )
            modules[i] = new AndGate  <IdType, ValueType, SizeType>();
        else if (module_type == "xand"   )
            modules[i] = new XandGate <IdType, ValueType, SizeType>();
        else if (module_type == "nand"   )
            modules[i] = new NandGate <IdType, ValueType, SizeType>();
        else if (module_type == "add"    )
            modules[i] = new Adder    <IdType, ValueType, SizeType>();
        else if (module_type == "subtract")
            modules[i] = new Subtractor<IdType, ValueType, SizeType>();
        else if (module_type == "enable" )
            modules[i] = new Enabler   <IdType, ValueType, SizeType>();
        else if (module_type == "mux"    )
            modules[i] = new Mux       <IdType, ValueType, SizeType>();
        else if (module_type == "demux" )
            modules[i] = new Demux     <IdType, ValueType, SizeType>();
        else {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: "
                <<" Module type "<<module_type<<" invalid"<<endl;
            return 8;
        }
        retval = modules[i]->Readin(str);
        if (retval != 0) return retval;
        if (modules[i]->num_inputs > max_num_inputs) max_num_inputs = modules[i]->num_inputs;
        if (modules[i]->num_outputs > max_num_outputs) max_num_outputs = modules[i]->num_outputs;
        
        for (SizeType j=0; j<modules[i]->num_inputs; j++)
        {
            ret = net_map.insert(
                pair<string, IdType>(modules[i]->input_names[j], num_nets));
            if (ret.second != false) num_nets++;
            modules[i]->input_Ids[j] = ret.first->second;
        }
        for (SizeType j=0; j<modules[i]->num_outputs; j++)
        {
            ret = net_map.insert(
                pair<string, IdType>(modules[i]->output_names[j], num_nets));
            if (ret.second != false) num_nets++;
            modules[i]->output_Ids[j] = ret.first->second;
        }

        //cout<<modules[i]->name << ":" << modules[i]->type;
        //for (SizeType j=0; j<modules[i]->num_inputs; j++)
        //    cout<<" "<<modules[i]->input_names[j]<<","<<modules[i]->input_Ids[j];
        //cout<<" -> ";
        //for (SizeType j=0; j<modules[i]->num_outputs; j++)
        //    cout<<" "<<modules[i]->output_names[j]<<","<<modules[i]->output_Ids[j];
        //cout<<endl;
    }
    num_nets = net_map.size();
    cout<<"#Modules = "<<num_modules<<" #Nets = "<<num_nets<<endl;

    fin>>num_inputs;
    net_widths = new SizeType[num_nets];
    inputs = new IdType[num_inputs];
    memset(net_widths, 0, sizeof(SizeType) * num_nets);
    for (SizeType i=0; i<num_nets; i++)
    {
        string str;
        fin>>str;
        transform(str.begin(), str.end(), str.begin(), ::tolower);
        map<string, IdType>::iterator it = net_map.find(str);
        if (it == net_map.end())
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: "
                <<" Net "<<str<<" cannot be found"<<endl;
            return 9;
        }
        fin>>net_widths[it->second];
        if (i<num_inputs) inputs[i] = it->second;
    }
    fin.close(); 
    return 0;
}

int MakeOrder()
{
    //IdType *net_from_module = new IdType[num_nets];
    //IdType *net_from_output = new IdType[num_nets];
    //IdType *net_to_module   = new IdType[num_nets];
    //IdType *net_to_input    = new IdType[num_nets];
    IdType   *net_order         = new IdType[num_nets];
    IdType   *net_order_inv     = new IdType[num_nets];
    IdType   *module_order      = new IdType[num_modules];
    IdType   *module_order_inv  = new IdType[num_modules];
    SizeType *module_in_counter = new SizeType[num_modules];
    SizeType net_counter    = 0;
    SizeType module_counter = 0;

    nets = new NetT*[num_nets];
    for (SizeType i=0; i<num_nets; i++)
    {
        NetT* net = new NetT;
        nets[i] = net;
        net->from_module = NULL_Id;
        net->from_output = NULL_Id;
        net->to_module   = NULL_Id;
        net->to_input    = NULL_Id;
        net_order      [i] = NULL_Id;
        net_order_inv  [i] = NULL_Id;
    }
    for (SizeType i=0; i<num_modules; i++)
    {
        module_order     [i] = NULL_Id;
        module_order_inv [i] = NULL_Id;
        module_in_counter[i] = modules[i]->num_inputs;
    }

    for (SizeType i=0; i<num_modules; i++)
    {
        ModuleT *module = modules[i];
        for (SizeType j=0; j<module->num_inputs; j++)
        {
            NetT* net = nets[module->input_Ids[j]];
            if (net->to_module != NULL_Id)
            {
                cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<< module->name
                    <<" net "<< module->input_names[j]
                    <<" already connected as input to module "
                    << modules[net->to_module]->name << " input " 
                    << net->to_input << ". Use a cross."<<endl;
                return 10;
            }
            net->to_module = i;
            net->to_input  = j;
            net->name      = module->input_names[j];
        }

        for (SizeType j=0; j<module->num_outputs; j++)
        {
            NetT* net = nets[module->output_Ids[j]];
            if (net->from_module != NULL_Id)
            {
                cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<< module->name
                    <<" net "<< module->output_names[j]
                    <<" already connected as output to module "
                    << modules[net->from_module]->name << " input " 
                    << net->from_output << "."<<endl;
                return 11;
            }
            net->from_module = i;
            net->from_output = j;
            net->name        = module->output_names[j];
        }
    }

    SizeType *temp_widths = new SizeType[num_nets];
    memcpy(temp_widths, net_widths, sizeof(SizeType) * num_nets);
    cout<<"Inputs :";
    //for (SizeType i=0; i<num_nets; i++)
    for (SizeType i=0; i<num_inputs; i++)
    {
        NetT *net = nets[inputs[i]];
        //NetT *net = nets[i];
        if (net->from_module == NULL_Id)
        {
            /*if (net->to_module == NULL_Id)
            {
                cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ net "<<net
                    <<" no connection"<<endl;
                return 12;
            }*/
            net_order[net_counter] = inputs[i];
            net_order_inv[inputs[i]] = net_counter;
            net_counter ++;
            net_widths[net_counter] = temp_widths[inputs[i]];
            cout<<" "<<net->name<<","<<net_widths[net_counter];
        }
    }
    cout<<endl;
    if (net_counter == 0) 
    {
        cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]:"
            <<" no input"<<endl;
        return 13;
    }

    cout<<"Outputs :";
    SizeType net_current = 0;
    while (net_current < net_counter)
    {
        NetT* net = nets[net_order[net_current]];
        if (net->to_module == NULL_Id)
        {
            cout<<" "<<net->name;
        } else {
            IdType module_Id = net->to_module;
            module_in_counter[module_Id]--;
            if (module_in_counter[module_Id] == 0)
            {
                module_order[module_counter] = module_Id;
                module_order_inv[module_Id] = module_counter;
                module_counter++;

                ModuleT *module = modules[module_Id];
                for (SizeType j=0; j<module->num_outputs; j++)
                {
                    net_order[net_counter] = module->output_Ids[j];
                    net_order_inv[module->output_Ids[j]] = net_counter;
                    net_widths[net_counter] = temp_widths[module->output_Ids[j]];
                    net_counter ++;
                }
            }
        }
        net_current ++;
    }
    cout<<endl;

    NetT **temp_nets = new NetT*[num_nets];
    memcpy(temp_nets, nets, sizeof(NetT*) * num_nets);
    cout<<"Nets :"<<endl;
    for (SizeType i=0; i<num_nets; i++)
    {
        NetT* net = temp_nets[net_order[i]];
        nets[i] = net;
        cout<<"  "<<i<<","<<net->name<<","<<net_widths[i]<<" : ";
        if (net->from_module != NULL_Id) 
        {
            cout<< net->from_module << ","
                << modules[net->from_module]->name<< ","
                << net->from_output;
            net->from_module = module_order_inv[net->from_module];
        }
        cout<<" -> ";
        if (net->to_module != NULL_Id) 
        {
            cout<< net->to_module << ","
                << modules[net->to_module]->name<<","
                << net->to_input;
            net->to_module = module_order_inv[net->to_module];
        }
        cout<<endl;
    }

    ModuleT **temp_modules = new ModuleT*[num_modules];
    memcpy(temp_modules, modules, sizeof(ModuleT*) * num_modules);
    cout<<"Modules :"<<endl;
    for (SizeType i=0; i<num_modules; i++)
    {
        ModuleT* module = temp_modules[module_order[i]];
        modules[i] = module;
        module->affects = new int[num_modules];
        memset(module->affects, 0, sizeof(int) * num_modules);
        cout<<"  "<<i<<","<<module->name<<" : "<<module->type;
        for (SizeType j=0; j<module->num_inputs; j++)
        {
            module->input_Ids[j] = net_order_inv[module->input_Ids[j]];
            cout<<" "<<module->input_Ids[j]<<","<<nets[module->input_Ids[j]]->name;
        }
        cout<<" ->";
        for (SizeType j=0; j<module->num_outputs; j++)
        {
            module->output_Ids[j] = net_order_inv[module->output_Ids[j]];
            cout<<" "<<module->output_Ids[j]<<","<<nets[module->output_Ids[j]]->name;
        }
        cout<<endl;
    }

    /*delete[] net_from_module; net_from_module = NULL;
    delete[] net_from_output; net_from_output = NULL;
    delete[] net_to_module  ; net_to_module   = NULL;
    delete[] net_to_input   ; net_to_input    = NULL;*/
    return 0;
}

int GetDependency()
{
    int retval = 0;

    #pragma omp parallel
    {
        int thread_num  = omp_get_thread_num();
        int num_threads = omp_get_num_threads();
        IdType i        = thread_num;
        IdType *queue   = new IdType[num_modules];
        int    *markers = new int   [num_modules];
        while (i < num_modules)
        {
            ModuleT* module = modules[i];
            memset(markers, 0, sizeof(int)*num_modules);
            queue[0] = i; markers[0] = 1;
            SizeType counter  = 1; 
            SizeType current  = 0;
            
            while (current < counter)
            {
                ModuleT* module1 = modules[queue[current]];
                markers[queue[current]] = 2;
                for (SizeType j=0; j<module1->num_outputs; j++)
                {
                    IdType neibor_Id = module1->output_Ids[j];
                    if (neibor_Id == NULL_Id) continue;
                    neibor_Id = nets[neibor_Id]->to_module;
                    if (neibor_Id == NULL_Id) continue;
                    if (markers[neibor_Id] != 0) continue;
                    markers[neibor_Id] = 1;
                    queue[counter] = neibor_Id;
                    module->affects[neibor_Id] = 1;
                    //printf("%d -> %d\n", i, neibor_Id);
                    //printf("%s -> %s\n", module->name.c_str(), modules[neibor_Id]->name.c_str());
                    counter ++;
                }
                current++;
            }
            i+=num_threads;
        }
        delete[] queue; queue=NULL;
        delete[] markers; markers = NULL;
    }

    cout<<"Affects matrix :"<<endl;
    for (IdType j=0; j<num_modules; j++) cout<<"\t"<<modules[j]->name;
    cout<<endl;
    for (IdType i=0; i<num_modules; i++)
    {
        ModuleT *module = modules[i];
        cout<<module->name;
        for (IdType j=0; j<num_modules; j++)
            cout<<"\t"<<module->affects[j];
        cout<<endl;
    }
    return retval;
}

int Evaluate(IdType     module_Id,
             SizeType   input_num,
             ValueType *inputs,
             ValueType *outputs,
             NetVT     *net_values_FF,
             NetVT     *net_values_E,
             IdType     input_tree_Id,
             ForestT   *forest,
             int        thread_num)
{
    int retval = 0;
    ModuleT* module = modules[module_Id];
    
    if (input_num == module->num_inputs)
    {
        retval = module->Evaluate(inputs, outputs);
        for (SizeType i=0; i<module->num_outputs; i++)
        {
            IdType output_Id = module->output_Ids[i];
            NetVT* output_values = (net_values_E == NULL) ? &net_values_FF[output_Id] : &net_values_E[output_Id];
            MapT*  output_map = &(output_values->root_Ids);
            MapTI  it_map = output_map->find(outputs[i]);
            if (it_map == output_map->end())
            {
                output_map->insert(MapTP(outputs[i], input_tree_Id));
            } else {
                IdType temp_Id = NULL_Id;
                retval = forest->AddTree(thread_num, it_map->second, input_tree_Id, temp_Id, 0);
                if (retval != 0) return 0;
                it_map->second = temp_Id;
            }
        }
    } else {
        IdType input_Id = module->input_Ids[input_num];
        NetVT* input_values = (net_values_E == NULL) ? &net_values_FF[input_Id] : &net_values_E[input_Id];
        MapT*  input_map = &(input_values->root_Ids);
        IdType error_sum = NULL_Id;
        MapTI  it_map;
        if (net_values_E == NULL)
        {
            for (it_map = input_map->begin(); it_map != input_map->end(); it_map++)
            {
                if (it_map == input_map->begin()) error_sum = it_map->second;
                else {
                    IdType temp_Id;
                    retval = forest->AddTree(thread_num, error_sum, it_map->second, temp_Id, 0);
                    if (retval != 0) return retval;           
                    error_sum = temp_Id;
                }
            }
        }
        for (it_map = input_map->begin(); it_map != input_map->end(); it_map++)
        {
            inputs[input_num] = it_map->first;
            IdType next_tree_Id = NULL_Id;
            if (net_values_E == NULL)
            {
                if (input_tree_Id != NULL_Id)
                    retval = forest->AndTree(thread_num, input_tree_Id, it_map->second, next_tree_Id, 0);
                else next_tree_Id = it_map->second;
            } else {
                MapTI it_temp = net_values_FF[input_Id].root_Ids.find(it_map->first);
                IdType next_tree_Id1 = NULL_Id;
                IdType next_tree_Id2 = NULL_Id;
                if (it_temp != net_values_FF[input_Id].root_Ids.end())
                {
                    retval = forest->SubTree(thread_num, it_temp->second, error_sum, next_tree_Id1, 0);
                    if (retval != 0) return retval;
                    if (next_tree_Id1 != NULL_Id) 
                        retval = forest->AddTree(thread_num, next_tree_Id1, it_map->second, next_tree_Id2, 0);
                    else next_tree_Id2 = it_map->second;
                } else next_tree_Id2 = it_map->second;
                if (input_tree_Id != NULL_Id)
                    retval = forest->AndTree(thread_num, input_tree_Id, next_tree_Id2, next_tree_Id, 0);
                else next_tree_Id = next_tree_Id2;
            }
            if (retval !=0) return retval;
            if (next_tree_Id != NULL_Id)
            {
                retval = Evaluate(module_Id, input_num+1, inputs, outputs,
                    net_values_FF, net_values_E, next_tree_Id, forest, thread_num);
                return retval;
            }
       }
    }
    return retval;
}

int Evaluate(SizeType  num_modules_TE,
             IdType   *module_Ids_TE,
             NetVT    *net_values_FF,
             NetVT    *net_values_E,
             ForestT  *forest,
             int       thread_num)
{
    int retval = 0;

    for (SizeType i=0; i<num_modules_TE; i++)
    {
        retval = Evaluate(module_Ids_TE[i], 0, 
            thread_inputs[thread_num], thread_outputs[thread_num], 
            net_values_FF, net_values_E, NULL_Id, forest, thread_num);
        if (retval != 0) return retval;
    }
    return retval;
}

int GetModuleList(IdType start_module_Id, SizeType& module_count, IdType *module_Ids)
{
    module_count =1;
    module_Ids[0] = start_module_Id;
    ModuleT* module = modules[start_module_Id];
    for (IdType i = start_module_Id+1; i<num_modules; i++)
    {
        if (module->affects[i])
        {
            module_Ids[module_count] = i;
            module_count ++;
        }
    }
    return 0;
}

void AddError(IdType type, IdType Id, IdType ref = NULL_Id, IdType cond = NULL_Id)
{                    
    error_types.insert(pair<IdType, IdType>(num_errors, type));
    error_Ids  .insert(pair<IdType, IdType>(num_errors, Id  ));
    error_refs .insert(pair<IdType, IdType>(num_errors, ref ));
    error_conds.insert(pair<IdType, IdType>(num_errors, cond));
    num_errors ++;
}

bool IsNetDepend(IdType Id_A, IdType Id_B)
{
    if (Id_A == NULL_Id || Id_B == NULL_Id) return false;
    if (Id_A == Id_B) return true;
    if (nets[Id_A]->to_module == NULL_Id) return false;
    if (nets[Id_B]->from_module == NULL_Id) return false;
    if (modules[nets[Id_A]->to_module]->affects[nets[Id_B]->from_module] != 0) return true;
    return false;
}

int GenerateErrors()
{
    // Generate the error list
    num_errors = 0;
    // Net errors
    for (IdType i=0; i<num_nets; i++)
    {
        for (IdType j=0; j<NUM_NET_ERRORS; j++)
        {
            if        (j==0) // Bus line stuck
            {
                for (ValueType k=0; k<(1<<net_widths[i]); k++)
                    AddError(j, i, k);
            } else if (j==1) // Bus order error
            {
                AddError(j, i);
            } else if (j==2) // Bus source error
            {
                for (IdType k=0; k<num_nets; k++)
                {
                    if (IsNetDepend(i, k)) continue;
                    AddError(j, i, k); 
                }
            } else if (j==3) // Bus count error
            {
                for (ValueType k=0; k<net_widths[i]; k++)
                    AddError(j, i, k);
            } else if (j==4) // conditional bus stuck line
            {
                for (IdType cond=0; cond < num_nets; cond++)
                {
                    if (IsNetDepend(i, cond)) continue;
                    for (ValueType k=0; k<(1<<net_widths[i]); k++)
                        AddError(j, i, k, cond);
                }
            } else if (j==5) // conditional bus order error
            {
                for (IdType cond=0; cond < num_nets; cond++)
                {
                    if (IsNetDepend(i, cond)) continue;
                    AddError(j, i, NULL_Id, cond);
                }
            } else if (j==6) // conditional bus source error
            {
                for (IdType cond=0; cond < num_nets; cond++)
                {
                    if (IsNetDepend(i, cond)) continue;
                    for (IdType k=0; k < num_nets; k++)
                    {
                        if (IsNetDepend(i, k)) continue;
                        AddError(j, i, k, cond);
                    }
                }
            }
        }
    }
    for (IdType i=0; i<num_modules; i++)
    {
        ModuleT *module = modules[i];
        string module_type = module -> type;
        for (IdType j=0; j<NUM_MODULE_ERRORS; j++)
        {
            if (j==0) // By-pass module
            {
                for (SizeType k=0; k<module->num_inputs; k++)
                for (SizeType p=0; k<module->num_outputs; p++)
                if (net_widths[module->input_Ids[k]] == net_widths[module->output_Ids[p]])
                    AddError(j + NUM_NET_ERRORS, i, k, p);
            } else if (j==1) // Adding not to outputs
            {
                for (SizeType k=0; k<modules[i]->num_outputs; k++)
                    AddError(j + NUM_NET_ERRORS, i, k);
            } else if (j==2) // Module substituation
            {
                bool is_interchangable = false;
                for (SizeType k=0; k<num_interchangable_types; k++)
                    if (module_type == interchangable_types[k])
                    {
                        is_interchangable = true;
                        break;
                    }
                if (is_interchangable)
                {
                    for (SizeType k=0; k<num_interchangable_types; k++)
                    if (module_type != interchangable_types[k])
                    {
                        AddError(j + NUM_NET_ERRORS, i, k); 
                    }
                }
            }
        }
    }

    return 0;
}

ValueType Reverse(ValueType a)
{
    char bits[sizeof (ValueType) * 8];
    SizeType length = 0;

    while (a!=0)
    {
        bits[length] = a%2;
        a /=2;
        length ++;
    }

    ValueType b = 0;
    for (SizeType i=0; i<length; i++)
    {
        b = b*2 + bits[i];
    }
    return b;
}

int ProcessError(
    IdType error_type,
    IdType error_Id,
    IdType error_ref,
    IdType error_cond,
    NetVT* net_values_FF,
    NetVT* net_values_E,
    IdType &start_module_Id,
    ForestT* forest,
    int thread_num)
{
    int retval = 0;
    start_module_Id = NULL_Id;

    IdType sum_Id2 = NULL_Id;
    MapT* map2 = &net_values_FF[error_cond].root_Ids;
    for (ValueType i=0; i<1; i++)
    {
        MapTI it = map2->find( i==0? 0 : (1<<net_widths[error_ref]) -1);
        if (it != map2->end())
        {
            if (sum_Id2 == NULL_Id) sum_Id2 = it->second;
            else {
                IdType temp_Id = NULL_Id;
                forest->AddTree(thread_num, it->second, sum_Id2, temp_Id, 0);
            }
        }
    }
    if (sum_Id2 == NULL_Id) return retval;


    switch (error_type)
    {
        case 0: // Bus line struck
        {
            net_values_E[error_Id].root_Ids.clear();
            IdType sum_Id = NULL_Id;
            MapT* map = &net_values_FF[error_Id].root_Ids;
            for (MapTI it = map->begin(); it != map->end(); it++)
            {
                if (sum_Id == NULL_Id) sum_Id = it->second;
                else {
                    IdType sum_Id2 = NULL_Id;
                    forest->AddTree(thread_num, it->second, sum_Id, sum_Id2, 0);
                    sum_Id = sum_Id2;
                }
            }
            net_values_E[error_Id].root_Ids.insert(MapTP(error_ref, sum_Id));
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 1: // Bus order error
        {
            //IdType sum_Id = NULL_Id;
            MapT* map_ff = &net_values_FF[error_Id].root_Ids;
            MapT* map_e  = &net_values_E [error_Id].root_Ids;
            map_e->clear();
            for (MapTI it = map_ff->begin(); it != map_ff->end(); it++)
            {
                map_e->insert(MapTP(Reverse(it->first), it->second));
            }
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 2: // Bus source error
        {
            //IdType sum_Id = NULL_Id;
            MapT* map_ff = &net_values_FF[error_ref].root_Ids;
            MapT* map_e  = &net_values_E [error_Id].root_Ids;
            map_e->clear();
            for (MapTI it = map_ff->begin(); it != map_ff->end(); it++)
            {
                map_e->insert(MapTP(it->first, it->second));
            }
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 3: // Bus count error
        {
            //IdType sum_Id = NULL_Id;
            MapT* map_ff = &net_values_FF[error_Id].root_Ids;
            MapT* map_e  = &net_values_E [error_Id].root_Ids;
            map_e->clear();
            for (MapTI it_ff = map_ff->begin(); it_ff != map_ff->end(); it_ff++)
            {
                ValueType value = it_ff->first & ((1<<error_ref)-1);
                MapTI it = map_ff->find(value);
                if (it == map_ff->end())
                    map_e->insert(MapTP(value, it_ff->second));
                else {
                    IdType temp_Id = NULL_Id;
                    retval = forest->AddTree(thread_num, it_ff->second, it->second, temp_Id, 0);
                    it->second = temp_Id;
                }
            }
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 4: // conditional bus stuck line
        {
            net_values_E[error_Id].root_Ids.clear();
            IdType sum_Id = NULL_Id;
            MapT* map = &net_values_FF[error_Id].root_Ids;
            for (MapTI it = map->begin(); it != map->end(); it++)
            {   
                if (sum_Id == NULL_Id) sum_Id = it->second;
                else {
                    IdType sum_Id2 = NULL_Id;
                    forest->AddTree(thread_num, it->second, sum_Id, sum_Id2, 0); 
                    sum_Id = sum_Id2;
                }   
            }
            if (sum_Id == NULL_Id) break;

            IdType temp_Id = NULL_Id;
            forest->AndTree(thread_num, sum_Id, sum_Id2, temp_Id, 0);
            if (temp_Id != NULL_Id)
            {
                net_values_E[error_Id].root_Ids.insert(MapTP(error_ref,temp_Id));
            }
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 5: // conditional bus order error
        {
            MapT* map_ff = &net_values_FF[error_Id].root_Ids;
            MapT* map_e  = &net_values_E [error_Id].root_Ids;
            map_e->clear();
            for (MapTI it = map_ff->begin(); it != map_ff->end(); it++)
            {   
                IdType temp_Id = NULL_Id;
                forest->AndTree(thread_num, it->second, sum_Id2, temp_Id, 0);
                if (temp_Id == NULL_Id) continue;
                map_e->insert(MapTP(Reverse(it->first), temp_Id));
            }   
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case 6: // conditional bus source error
        {
            //IdType sum_Id = NULL_Id;
            MapT* map_ff = &net_values_FF[error_ref].root_Ids;
            MapT* map_e  = &net_values_E [error_Id].root_Ids;
            map_e->clear();
            for (MapTI it = map_ff->begin(); it != map_ff->end(); it++)
            {
                IdType temp_Id = NULL_Id;
                forest->AndTree(thread_num, it->second, sum_Id2, temp_Id, 0);
                if (temp_Id == NULL_Id) continue;  
                map_e->insert(MapTP(it->first, temp_Id));
            }
            start_module_Id = nets[error_Id]->to_module;
            break;
        }
        case NUM_NET_ERRORS + 0: // bypass module
        {
            ModuleT* module = modules[error_Id];
            MapT* map_input = &net_values_FF[module->input_Ids[error_ref]].root_Ids;
            MapT* map_output = &net_values_E[module->output_Ids[error_cond]].root_Ids;
            map_output->clear();
            
            for (MapTI it= map_input->begin(); it!= map_input->end(); it++)
            {
                map_output->insert(MapTP(it->first, it->second));
            }
            start_module_Id = nets[module->output_Ids[error_cond]]->to_module;
            break;
        }
        case NUM_NET_ERRORS + 1: // Add not to outputs
        {
            ModuleT* module = modules[error_Id];
            IdType net_Id = module->output_Ids[error_ref];
            MapT* map_ff = &net_values_FF[net_Id].root_Ids;
            MapT* map_e  = &net_values_E [net_Id].root_Ids;
            map_e->clear();
           
            for (MapTI it = map_ff->begin(); it != map_ff->end(); it++)
            {
                map_e->insert(MapTP(~(it->first), it->second));
            }
            start_module_Id = nets[net_Id]->to_module;
            break;
        }
        case NUM_NET_ERRORS + 2: // Module substituation
        {
            
            break;
        }
        default: break;
    }
    return retval;
}

int main(int argc, char* argv[])
{
    int retval = 0;
    ForestT* forest = NULL;
    NetVT*   net_values_FF = NULL;
    int* retvals = NULL;
    IdType** thread_modules = NULL;
    NetVT**  net_values_E = NULL;

    do
    {
        if ((retval = ReadInput(argc, argv))) break;
        if ((retval = MakeOrder()          )) break;
        if ((retval = GetDependency()      )) break;

        #pragma omp parallel
        {
            int thread_num = omp_get_thread_num();
            int num_threads = omp_get_num_threads();

            #pragma omp single
            {
                do
                {
                    retvals = new int[num_threads];
                    memset(retvals, 0, sizeof(int) * num_threads);
                    forest = new ForestT;
                    if ((retvals[thread_num] = forest->Init(num_threads, num_inputs, net_widths))) break;
                    thread_inputs = new ValueType*[num_threads];
                    thread_outputs = new ValueType*[num_threads];
                    thread_modules = new IdType*[num_threads];
                    for (SizeType i=0; i<num_threads; i++)
                    {
                        thread_inputs[i] = new ValueType[max_num_inputs];
                        thread_outputs[i] = new ValueType[max_num_outputs];
                        thread_modules[i] = new IdType[num_modules];
                    } 

                    net_values_FF = new NetVT[num_nets];
                    //prepare the inputs
                    for (SizeType i=0; i<num_inputs; i++)
                    {
                        MapT* map = &net_values_FF[i].root_Ids;
                        for (ValueType j=0; j< (1<<net_widths[i]); j++)
                        {
                            IdType temp_Id = NULL_Id;
                            retvals[thread_num] = forest->NewTree(thread_num, i, j, temp_Id);
                            if (retvals[thread_num] != 0) break;
                            map->insert(MapTP(j, temp_Id));
                        }
                    }
                    for (IdType i=0; i<num_modules; i++)
                        thread_modules[thread_num][i] = i;

                    // Evaluate the FF cicuit
                    if ((retvals[thread_num] = Evaluate(num_modules, thread_modules[thread_num], net_values_FF, NULL, forest, thread_num))) break;

                    if ((retvals[thread_num] = GenerateErrors())) break;
                    net_values_E = new NetVT*[num_errors];
                    for (SizeType i=0; i<num_errors; i++)
                        net_values_E[i] = new NetVT[num_nets];
                } while(0);

            }

            #pragma omp for
            for (IdType i=0; i<num_errors; i++)
            {
                if (retvals[thread_num] != 0) continue;
                IdType error_type = error_types.find(i)->second;
                IdType error_Id   = error_Ids  .find(i)->second;
                IdType error_ref  = error_refs .find(i)->second;
                IdType error_cond = error_conds.find(i)->second;
                IdType start_module_Id = NULL_Id;
                SizeType module_count = 0;
                
                // Place the error
                if ((retvals[thread_num] = ProcessError(error_type, error_Id, error_ref, error_cond, 
                    net_values_FF, net_values_E[i], start_module_Id, forest, thread_num))) continue;
                if (start_module_Id == NULL_Id) continue;

                // Get list of modules to evaluate
                if ((retvals[thread_num] = GetModuleList(start_module_Id, module_count, thread_modules[thread_num]))) continue;
                // Evaluate the faulty circuit
                if ((retvals[thread_num] = Evaluate(module_count, thread_modules[thread_num], net_values_FF, net_values_E[i], forest, thread_num))) continue;
            } 

            if (retvals[thread_num] != 0) cerr<<"Thread "<<thread_num<<" terminated with error code "<<retvals[thread_num]<<endl;
        }
    } while(0);

    if (retval != 0) cerr<<"Terminated with error code "<<retval<<endl;
    return retval;
}

