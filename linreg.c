#include <linreg.h>
#include <common.h>
#include <stddef.h>
#include <printf.h>

float train[5][2] = {
  {0, 0},
  {1, 2},
  {2, 4},
  {3, 6},
  {4, 8},
};

float cost_fn(float w1)
{
  float result = 0.0f;

  for (size_t i = 0; i < ARRAY_SIZE(train); ++i) {
    float x = train[i][0];
    float y = w1 * x;
    float err = y - train[i][1];
    result += err * err;
  }
  return result / ARRAY_SIZE(train);
}

void linear_regression_test(void)
{
  size_t i;
  float w1 = 3.0f;
  float beta = 3.0f;
  float delta = 0.01;
  float rate = 0.03;
  float deriv;

  for (i = 0; i < 1000; ++i) {
    float c1 = cost_fn(w1);
    float c2 = cost_fn(w1 + delta);
    deriv = (c2 - c1) / delta;
    w1 += deriv * rate;
    printf("%f %f\r\n", w1, 0.434f);
  }
}
