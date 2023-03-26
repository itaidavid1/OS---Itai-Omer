//
// Created by itai on 22/03/2023.
//
#include "osm.h"
#include <iostream>
#include <string>
#include <math.h>
using namespace std;


int main() {
  double op_time ;
  double empty_func_time ;
  double osm_call_time ;
  unsigned int iterations = pow(10,5);

  op_time = osm_operation_time(iterations);
  empty_func_time = osm_function_time(iterations);
//  osm_call_time = osm_syscall_time(iterations);

//  std::cout << "running " + std::to_string(iterations) + " iterations" << std::endl ;
  std::cout << "operation : " + std::to_string(op_time) + " nns" << std::endl ;
//  std::cout << "empty functions call : " + std::to_string(empty_func_time) + " nns" << std::endl ;
//  std::cout << "osm call : " + std::to_string(osm_call_time) + " nns" << std::endl ;
  std::cout << "osm call" << std::endl;
  const char*nul = nullptr;
  std::cout << "before "<< nul << "after\n";
  return 0;
}