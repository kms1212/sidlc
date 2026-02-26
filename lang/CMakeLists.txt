cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

target_sources(sidlc PUBLIC ast.cc c_header_generator.cc c_source_generator.cc lexer.cc parser.cc)
