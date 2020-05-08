#pragma once

#include <sys/types.h>

#include <vector>

typedef double Float;

// Binary is a JS-bound byte array
typedef std::vector<uint8_t> Binary;

// Neural Network: to avoid including MiniDNN.h we use a forward declaration
namespace MiniDNN {
class Network;
}

typedef MiniDNN::Network NeuralNetwork;

