
#include <string>
#include <map>

using namespace std;

template<
    typename IdType>
class Net
{
public:
    string      name       ; // Name of the net
    IdType      from_module; // Which module outputs to the net
    IdType      from_output;
    IdType      to_module  ; // Which module accepts the net as input
    IdType      to_input   ;
    //int         width      ; 
};

template<
    typename ValueType,
    typename SizeType>
class InputValue
{
public:
    SizeType    num_values ; // Number of possible values of this input to achieve a specific value on a given net
    ValueType  *values     ; // The input values
    InputValue *childs     ; // Pointers to the childs describing values of the next input
};

template<
    typename IdType,
    typename ValueType,
    typename SizeType>
class NetValue
{
public:
    /*SizeType    num_values ; // Number of possible values of the net
    char       *markers    ; // The markers for the values (0: normal, 1: equal to another net, 2: flip from another net
    ValueType  *values     ; // The possible values
    IdType     *references ; // References to other nets if mode is 1 or 2
    IdType     *input_root_Ids; // The input value trees describing all possible inputs to achieve the given value on the net
    */
    map<ValueType, IdType> root_Ids;
};

