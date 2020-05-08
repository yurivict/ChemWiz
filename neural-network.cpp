
#include "mytypes.h"
#include "js-support.h"

#include <MiniDNN.h> // full MiniDNN

const char *TAG_NeuralNetwork = "NeuralNetwork";

namespace JsBinding {

namespace JsNeuralNetwork {

void xnewo(js_State *J, NeuralNetwork *b) {
  js_getglobal(J, TAG_NeuralNetwork);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_NeuralNetwork, b, [](js_State *J, void *p) {
    delete (NeuralNetwork*)p;
  });
}

void init(js_State *J) {
  using namespace MiniDNN;
  JsSupport::beginDefineClass(J, TAG_NeuralNetwork, [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new NeuralNetwork);
  });
  { // methods
    // layer: Convolutional<Act>: (in_width, in_height, in_channels, out_channels, window_width, window_height)
    #define ADD_METHOD_LAYER_CONVOLUTIONAL(ActivationFn) \
      ADD_METHOD_CPP(NeuralNetwork, addLayerConvolutional##ActivationFn, { \
        AssertNargs(6) \
        GetArg(NeuralNetwork, 0)->add_layer(new Convolutional<ActivationFn>(GetArgUInt32(1), GetArgUInt32(2), GetArgUInt32(3), GetArgUInt32(4), GetArgUInt32(5), GetArgUInt32(6))); \
        ReturnVoid(J); \
      }, 0)
    ADD_METHOD_LAYER_CONVOLUTIONAL(Identity)
    ADD_METHOD_LAYER_CONVOLUTIONAL(ReLU)
    ADD_METHOD_LAYER_CONVOLUTIONAL(Tanh)
    ADD_METHOD_LAYER_CONVOLUTIONAL(Sigmoid)
    ADD_METHOD_LAYER_CONVOLUTIONAL(Softmax)
    ADD_METHOD_LAYER_CONVOLUTIONAL(Mish)
    #undef ADD_METHOD_LAYER_CONVOLUTIONAL

    // layer: MaxPooling<Act>: (in_width, in_height, in_channels, pooling_width, pooling_height)
    #define ADD_METHOD_LAYER_MAX_POOL(ActivationFn) \
      ADD_METHOD_CPP(NeuralNetwork, addLayerMaxPool##ActivationFn, { \
        AssertNargs(5) \
        GetArg(NeuralNetwork, 0)->add_layer(new MaxPooling<ActivationFn>(GetArgUInt32(1), GetArgUInt32(2), GetArgUInt32(3), GetArgUInt32(4), GetArgUInt32(5))); \
        ReturnVoid(J); \
      }, 0)
    ADD_METHOD_LAYER_MAX_POOL(Identity)
    ADD_METHOD_LAYER_MAX_POOL(ReLU)
    ADD_METHOD_LAYER_MAX_POOL(Tanh)
    ADD_METHOD_LAYER_MAX_POOL(Sigmoid)
    ADD_METHOD_LAYER_MAX_POOL(Softmax)
    ADD_METHOD_LAYER_MAX_POOL(Mish)
    #undef ADD_METHOD_LAYER_MAX_POOL

    // layer: FullyConnected<Act>: (in_size, out_size)
    #define ADD_METHOD_LAYER_FULLY_CONNECTED(ActivationFn) \
      ADD_METHOD_CPP(NeuralNetwork, addLayerFullyConnected##ActivationFn, { \
        AssertNargs(2) \
        GetArg(NeuralNetwork, 0)->add_layer(new FullyConnected<ActivationFn>(GetArgUInt32(1), GetArgUInt32(2))); \
        ReturnVoid(J); \
      }, 0)
    ADD_METHOD_LAYER_FULLY_CONNECTED(Identity)
    ADD_METHOD_LAYER_FULLY_CONNECTED(ReLU)
    ADD_METHOD_LAYER_FULLY_CONNECTED(Tanh)
    ADD_METHOD_LAYER_FULLY_CONNECTED(Sigmoid)
    ADD_METHOD_LAYER_FULLY_CONNECTED(Softmax)
    ADD_METHOD_LAYER_FULLY_CONNECTED(Mish)
    #undef ADD_METHOD_LAYER_FULLY_CONNECTED

    // output
    #define ADD_METHOD_CPP_OUTPUT(LossFn) \
      ADD_METHOD_CPP(NeuralNetwork, addOutput##LossFn, { \
        AssertNargs(0) \
        GetArg(NeuralNetwork, 0)->set_output(new LossFn); \
        ReturnVoid(J); \
      }, 0)
    ADD_METHOD_CPP_OUTPUT(BinaryClassEntropy)
    ADD_METHOD_CPP_OUTPUT(MultiClassEntropy)
    ADD_METHOD_CPP_OUTPUT(RegressionMSE)
    #undef ADD_METHOD_CPP_OUTPUT

    ADD_METHOD_CPP(NeuralNetwork, initializeLayersWithRandonNumbers, {
      AssertNargs(3)
      GetArg(NeuralNetwork, 0)->init(
        GetArgUInt32(1)/*mu: Mean of the normal distribution*/,
        GetArgUInt32(2)/*sigma: Standard deviation of the normal distribution*/,
        GetArgUInt32(3)/*seed: Set the random seed of the RNG when >0, otherwise use the current random state*/
      );
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(NeuralNetwork, write, {
      AssertNargs(2)
      GetArg(NeuralNetwork, 0)->export_net(
        GetArgString(1),
        GetArgString(2)
      );
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(NeuralNetwork, read, {
      AssertNargs(2)
      GetArg(NeuralNetwork, 0)->read_net(
        GetArgString(1),
        GetArgString(2)
      );
      ReturnVoid(J);
    }, 0)
  }
  JsSupport::endDefineClass(J);
}

} // JsNeuralNetwork

} // JsBinding
