/*******************************************************
 * Copyright (c) 2014, ArrayFire
 * All rights reserved.
 *
 * This file is distributed under 3-clause BSD license.
 * The complete license agreement can be obtained at:
 * http://arrayfire.com/licenses/BSD-3-Clause
 ********************************************************/

#include <arrayfire.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <af/util.h>
#include <math.h>
#include "mnist_common.h"

using namespace af;

float accuracy(const array& predicted, const array& target)
{
    array val, plabels, tlabels;
    max(val, tlabels, target, 1);
    max(val, plabels, predicted, 1);

    return 100 * count<float>(plabels == tlabels) / tlabels.elements();
}

// Activation function
array sigmoid(const array &val)
{
    return 1 / (1 + exp(-val));
}

// Predict based on given parameters
array predict(const array &X, const array &Weights)
{
    return sigmoid(matmul(X, Weights));
}

void cost(array &J, array &dJ, const array &Weights,
          const array &X, const array &Y, double lambda = 1.0)
{
    // Number of samples
    int m = Y.dims(0);

    // Make the lambda corresponding to Weights(0) == 0
    array lambdat = constant(lambda, Weights.dims());
    lambdat(0, span) = 0;

    // Get the prediction
    array H = predict(X, Weights);

    // Calculate cost
    J =  -sum(Y * log(H) + (1 - Y) * log(1 - H)) / m;
    J = J + 0.5 * sum(lambdat * Weights * Weights) / m;

    // Find the gradient of cost
    array D = (H - Y);
    dJ = (matmul(X.T(), D) + lambdat * Weights) / m;
}

array train(const array &X, const array &Y,
            double alpha = 0.1, double lambda = 1.0, int maxiter = 1000)
{

    // Initialize parameters to 0
    array Weights = constant(0, X.dims(1), Y.dims(1));

    array J, dJ;
    for (int i = 0; i < maxiter; i++) {

        // Get the cost and gradient
        cost(J, dJ, Weights, X, Y, lambda);
        if (alltrue<bool>(J < 0.1)) break;

        // Update the parameters via gradient descent
        Weights = Weights - alpha * dJ;
    }

    return Weights;
}

void benchmark_lr(const array &train_feats,
                          const array &train_targets,
                          const array test_feats)
{
    timer::start();
    array Weights = train(train_feats, train_targets, 1.0, 1.0, 500);
    af::sync();
    printf("Training time: %4.4lf s\n", timer::stop());

    timer::start();
    const int iter = 100;
    for (int i = 0; i < iter; i++) {
        array test_outputs  = predict(test_feats , Weights);
        test_outputs.eval();
    }
    af::sync();
    printf("Prediction time: %4.4lf s\n", timer::stop() / iter);
}


// Demo of one vs all logistic regression
int lr_demo(bool console, int perc)
{
    array train_images, train_targets;
    array test_images, test_targets;
    int num_train, num_test, num_classes;

    // Load mnist data
    float frac = (float)(perc) / 100.0;
    setup_mnist<true>(&num_classes, &num_train, &num_test,
                      train_images, test_images,
                      train_targets, test_targets, frac);

    // Reshape images into feature vectors
    int feature_length = train_images.elements() / num_train;
    array train_feats = moddims(train_images, feature_length, num_train).T();
    array test_feats  = moddims(test_images , feature_length, num_test ).T();

    train_targets = train_targets.T();
    test_targets  = test_targets.T();

    // Add a bias that is always 1
    train_feats = join(1, constant(1, num_train, 1), train_feats);
    test_feats  = join(1, constant(1, num_test , 1), test_feats );

    // Train logistic regression parameters
    array Weights = train(train_feats, train_targets, 1.0, 1.0, 500);

    // Predict the results
    array train_outputs = predict(train_feats, Weights);
    array test_outputs  = predict(test_feats , Weights);

    printf("Accuracy on training data: %2.2f\n",
           accuracy(train_outputs, train_targets ));

    printf("Accuracy on testing data: %2.2f\n",
           accuracy(test_outputs , test_targets ));

    benchmark_lr(train_feats, train_targets, test_feats);

    if (!console) {
        test_outputs = test_outputs.T();
        // Get 20 random test images.
        display_results<true>(test_images, test_outputs, 20);
    }

    return 0;
}

int main(int argc, char** argv)
{
    int device   = argc > 1 ? atoi(argv[1]) : 0;
    bool console = argc > 2 ? argv[2][0] == '-' : false;
    int perc     = argc > 3 ? atoi(argv[3]) : 60;

    try {

        af::deviceset(device);
        af::info();
        return lr_demo(console, perc);

    } catch (af::exception &ae) {
        std::cout << ae.what() << std::endl;
    }

}
