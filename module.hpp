
#include <string>
#include <sstream>
#include <algorithm>

using namespace std;

template<
    typename IdType,
    typename ValueType,
    typename SizeType>
class NetValue;

string interchangable_types[] = {"OrGate", "XorGate", "NorGate", "AndGate", "NandGate", "XandGate"};
const int num_interchangable_types = 6;

template<
    typename IdType,
    typename ValueType,
    typename SizeType>
class Module
{
public:
    string    type        ; // Type of the module
    string    name        ; // Name of the module
    SizeType  bit_width   ; // Bit width (if applicable)
    SizeType  min_inputs  ; // minimum number of inputs
    SizeType  max_inputs  ; // maximum number of inputs
    SizeType  num_inputs  ; // Number of inputs to the module
    SizeType  min_outputs ; // minimum number of outputs
    SizeType  max_outputs ; // maximum number of outputs
    SizeType  num_outputs ; // Number of outputs to the module
    string   *input_names ; // Names of the inputs
    string   *output_names; // Names of the outputs
    IdType   *input_Ids   ; // Ids of the inputs
    IdType   *output_Ids  ; // Ids of the outputs
    int      *affects     ; // Whether this affects others

    Module() :
        type        ("undefined"),
        name        ("undefined"),
        bit_width   (0          ),
        min_inputs  (1          ),
        max_inputs  (-1         ),
        num_inputs  (0          ),
        min_outputs (1          ),
        max_outputs (1          ),
        num_outputs (0          ),
        input_names (NULL       ),
        output_names(NULL       ),
        input_Ids   (NULL       ),
        output_Ids  (NULL       ),
        //num_affected_modules(0  ),
        affects     (NULL   )
    {
    } // end Module()

    virtual ~Module()
    {
        if (input_names  != NULL) {delete[] input_names ; input_names  = NULL;}
        if (output_names != NULL) {delete[] output_names; output_names = NULL;}
        if (input_Ids    != NULL) {delete[] input_Ids   ; input_Ids    = NULL;}
        if (output_Ids   != NULL) {delete[] output_Ids  ; output_Ids   = NULL;}
        if (affects      != NULL) {delete[] affects     ; affects      = NULL;}
    } // end ~Module()

    int Init(
        string    type,
        string    name,
        SizeType  bit_width,
        SizeType  num_inputs,
        SizeType  num_outputs,
        string   *input_names,
        string   *output_names)
    {
        if (bit_width > (SizeType)sizeof(ValueType)*8) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<name
                <<" bit_width "<<bit_width<<" > "<<sizeof(ValueType)*8<<endl;
            return 3;
        }
        if (min_inputs!=-1 && num_inputs < min_inputs) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<name
                <<" #inputs "<<num_inputs<<" < "<<min_inputs<<endl;
            return 4;
        }
        if (max_inputs!=-1 && num_inputs > max_inputs) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<name
                <<" #inputs "<<num_inputs<<" > "<<max_inputs<<endl;
            return 5;
        }
        if (min_outputs!=-1 && num_outputs < min_outputs) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<name
                <<" #outputs "<<num_outputs<<" < "<<min_outputs<<endl;
            return 6;
        }
        if (max_outputs!=-1 && num_outputs > max_outputs) 
        {
            cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<name
                <<" #outputs "<<num_outputs<<" > "<<max_outputs<<endl;
            return 7;
        }

        this -> type         = type       ;
        this -> name         = name       ;
        this -> bit_width    = bit_width  ;
        this -> num_inputs   = num_inputs ;
        this -> num_outputs  = num_outputs;
        this -> input_names  = new string[num_inputs ];
        this -> output_names = new string[num_outputs];
        this -> input_Ids    = new IdType[num_inputs ];
        this -> output_Ids   = new IdType[num_outputs];
        transform(this->name.begin(), this->name.end(),
                  this->name.begin(), ::toupper);
        for (SizeType i=0; i<num_inputs; i++)
        {
            string str = input_names[i];
            transform(str.begin(), str.end(), str.begin(), ::tolower);
            this->input_names[i] = str;
            // this -> Input_Ids[i] = 
        }

        for (SizeType i=0; i<num_outputs; i++)
        {
            string str = output_names[i];
            transform(str.begin(), str.end(), str.begin(), ::tolower);
            this->output_names[i] = str;
            // this -> output_Ids[i] = 
        }

        return 0;
    } // end Init(...)

    virtual int Readin(string str)
    { 
        //cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<this->name
        //    <<" Readin undefined"<<endl;
        //return 1;
        return Readin(this->type, str);
    } // end Readin

    int Readin(string type, string str)
    {
        stringstream sinput;
        string name;
        SizeType bit_width = 0, num_inputs = 0, num_outputs = 0;
        sinput.str(str);
        sinput >> name >> bit_width >> num_inputs >> num_outputs;
        string* input_names = new string[num_inputs];
        string* output_names = new string[num_outputs];
        for (SizeType i=0; i<num_inputs; i++)
            sinput >> input_names[i];
        for (SizeType i=0; i<num_outputs; i++)
            sinput >> output_names[i];
        return Init(type, name, bit_width, num_inputs, num_outputs, 
            input_names, output_names);
    }

    virtual int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        cerr<<"Error ["<<__FILE__<<","<<__LINE__<<"]: @ "<<this->name
            <<" Evaluate undefined"<<endl;
        return 2;
    } // end Evaluate(...)
};

template<typename IdType, typename ValueType, typename SizeType>
class Connecter : public Module<IdType, ValueType, SizeType>
{
public:
    Connecter() { 
        this->type = "Connecter";
        this->max_inputs = 1;
        this->max_outputs = -1;
    }
    
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        for (SizeType i=0; i<this->num_outputs; i++)
            outputs[i] = inputs[0];
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class NotGate : public Module<IdType, ValueType, SizeType>
{
public:
    NotGate() {
        this->type = "NotGate";
        this->max_inputs  = 1;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        outputs[0] = ~inputs[0];
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class OrGate : public Module<IdType, ValueType, SizeType>
{
public:
    OrGate() {
        this->type = "OrGate";
        this->min_inputs  = 2;
    }
    
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = temp | inputs[i];
        outputs[0] = temp;
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class XorGate : public Module<IdType, ValueType, SizeType>
{
public:
    XorGate() {
        this->type = "XorGate";
        this->min_inputs  = 2;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = temp ^ inputs[i];
        outputs[0] = temp;
        return 0;
    }

};

template<typename IdType, typename ValueType, typename SizeType>
class NorGate : public Module<IdType, ValueType, SizeType>
{
public:
    NorGate() {
        this->type = "NorGate";
        this->min_inputs  = 2;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = temp | inputs[i];
        outputs[0] = ~temp;
        return 0;
    }

};

template<typename IdType, typename ValueType, typename SizeType>
class AndGate : public Module<IdType, ValueType, SizeType>
{
public:
    AndGate() {
        this->type = "AndGate";
        this->min_inputs  = 2;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = temp & inputs[i];
        outputs[0] = temp;
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class XandGate : public Module<IdType, ValueType, SizeType>
{
public:
    XandGate() {
        this->type = "XandGate";
        this->min_inputs  = 2;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = ~(temp ^ inputs[i]);
        outputs[0] = temp;
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class NandGate : public Module<IdType, ValueType, SizeType>
{
public:
    NandGate() {
        this->type = "NandGate";
        this->min_inputs  = 2;
    }

    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp = temp & inputs[i];
        outputs[0] = ~temp;
        return 0;
    }
};

template<typename IdType, typename ValueType, typename SizeType>
class Adder : public Module<IdType, ValueType, SizeType>
{
public:
    Adder() {
        this->type = "adder";
        this->min_inputs  = 2;
    }
   
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp += inputs[i];
        outputs[0] = temp;
        return 0;
    } 
};

template<typename IdType, typename ValueType, typename SizeType>
class Subtractor : public Module<IdType, ValueType, SizeType>
{
public:
    Subtractor() {
        this->type = "subtractor";
        this->min_inputs  = 2;
    }
   
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType temp=inputs[0];
        for (SizeType i=1; i<this->num_inputs; i++)
            temp -= inputs[i];
        outputs[0] = temp;
        return 0;
    } 
};

template<typename IdType, typename ValueType, typename SizeType>
class Enabler : public Module<IdType, ValueType, SizeType>
{
public:
    Enabler() {
        this->type = "enabler";
        this->min_inputs = 2;
        this->max_inputs = 2;
    }
   
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        if (inputs[0] == 0) outputs[0] = 0;
        else outputs[0] = inputs[1];
        return 0;
    } 
};


template<typename IdType, typename ValueType, typename SizeType>
class Mux : public Module<IdType, ValueType, SizeType>
{
public:
    Mux() {
        this->type = "mux";
        this->min_inputs  = 3;
    }
   
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType sel=inputs[0];
        outputs[0] = inputs[sel+1];
        return 0;
    } 
};

template<typename IdType, typename ValueType, typename SizeType>
class Demux : public Module<IdType, ValueType, SizeType>
{
public:
    Demux() {
        this->type = "demux";
        this->min_inputs = 2;
        this->max_inputs = 2;
        this->min_inputs = 2;
        this->max_outputs = -1;
    }
   
    int Evaluate(ValueType *inputs, ValueType *outputs)
    {
        ValueType sel=inputs[0];
        for (SizeType i=0; i<this->num_outputs; i++)
            outputs[i] = 0;
        outputs[sel+1] = inputs[1];
        return 0;
    } 
};

