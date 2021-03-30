// Copyright 2021 lamp
//
// Created by lamp on 17.03.2021.
//

#include <server.hpp>

int main(){
  server s("127.0.0.1", 8080);
  s.start();
}
