// This file is part of the 're2r' package for R.
// Copyright (C) 2016, Qin Wenfeng
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
// BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <tuple>
#include "../inst/include/re2r.h"


// [[Rcpp::depends(RcppParallel)]]
#include <RcppParallel.h>
using namespace RcppParallel;

inline size_t utf8_length(const char* s) {
    size_t len = 0;
    while (*s) len += (*s++ & 0xc0) != 0x80;
    return len;
}

inline size_t getUtf8CharSize(char ch) {
    return ((0xE5000000 >> ((ch >> 3) & 0x1E)) & 3) + 1;
}

SEXP toprotect_loc_matrix(vector<tuple<size_t,size_t>>& input){

    if (input.empty()){
        Shield<SEXP>  xs(Rf_allocMatrix(INTSXP, 1,2));
        SEXP x = xs;
        auto ptr = INTEGER(x);

        ptr[0] = NA_INTEGER;
        ptr[1] = NA_INTEGER;
        return x;
    }

    Shield<SEXP>  xs(Rf_allocMatrix(INTSXP, input.size(),2));
    SEXP x = xs;
    auto ptr = INTEGER(x);
    size_t index = 0;

    for (auto it = input.begin(); it!=input.end(); it++, index++){
        ptr[index + input.size() * 0] = get<0>(*it);
        ptr[index + input.size() * 1] = get<1>(*it);
    }
    return x;
}

struct LocateP : public Worker
{
    vector<string>& input;
    vector<tuple<size_t,size_t>>& output;
    RE2* tt;

    LocateP(vector<string>&  input_, vector<tuple<size_t,size_t>>& output_, RE2* tt_)
        : input(input_), output(output_), tt(tt_){}

    void operator()(std::size_t begin, std::size_t end) {
        std::transform(input.begin() + begin,
                       input.begin() + end,
                       output.begin() + begin,
                       [this](string& x) -> tuple<size_t,size_t>{

                           size_t headn = 0;
                           StringPiece str(x);
                           auto str_size = x.length();
                           size_t lastIndex = 0;
                           StringPiece match;

                           if (! tt->Match(str, lastIndex , str_size, RE2::UNANCHORED, &match, 1)) {
                               return make_tuple(NA_INTEGER,NA_INTEGER);
                           } else {
                                if (match.size()){
                                    string mstring = match.as_string();
                                    size_t len_mstring = utf8_length(mstring.c_str());
                                    string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();

                                    size_t len_head = utf8_length(headz.c_str());
                                    headn += len_head;

                                    size_t head_s = (size_t) headn+1 ;
                                    headn += len_mstring;

                                    size_t tail_s = (size_t) headn;
                                    return make_tuple(head_s, tail_s);
                                } else {
                                    string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();
                                    size_t len_head = utf8_length(headz.c_str());
                                    headn += len_head;
                                    return make_tuple(headn+1, headn);
                                }

                           }

                       });
    }
};


struct LocateAllP : public Worker
{
    vector<string>& input;
    vector<vector<tuple<size_t,size_t>>>& output;
    RE2* tt;

    LocateAllP(vector<string>&  input_, vector<vector<tuple<size_t,size_t>>>& output_, RE2* tt_)
        : input(input_), output(output_), tt(tt_){}

    void operator()(std::size_t begin, std::size_t end) {
        std::transform(input.begin() + begin,
                       input.begin() + end,
                       output.begin() + begin,
                       [this](string& x) -> vector<tuple<size_t,size_t>>{

                           StringPiece match;

                           StringPiece str(x);
                           size_t lastIndex = 0;
                           size_t headn = 0;
                           auto str_size = x.length();

                           vector<tuple<size_t,size_t>> res;

                           while ( lastIndex < str_size && tt->Match(str, lastIndex , str_size, RE2::UNANCHORED, &match, 1)){
                               if (match.size()){
                                   string mstring = match.as_string();
                                   size_t len_mstring = utf8_length(mstring.c_str());

                                   string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();

                                   size_t len_head = utf8_length(headz.c_str());
                                   headn += len_head;

                                   auto head_s = headn+1 ;
                                   headn += len_mstring;

                                   auto tail_s = headn;

                                   res.push_back(make_tuple(head_s, tail_s));

                                   lastIndex = match.data() - str.data() + match.size();
                               } else {

                                   string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();
                                   size_t len_head = utf8_length(headz.c_str());
                                   headn += len_head;
                                   res.push_back(make_tuple(headn, headn-1));

                                   lastIndex = match.data() - str.data() + match.size();
                                   size_t sym_size = getUtf8CharSize(str.data()[lastIndex]);
                                   headn+=1;
                                   lastIndex += sym_size;
                               }
                           }

                           return res;
                       });
    }
};

// [[Rcpp::export]]
SEXP cpp_locate(CharacterVector input, XPtr<RE2>& regexp, bool all, bool parallel){
    string errmsg;

    auto ptr = regexp.checked_get();
    SEXP inputx = input;

    if (! parallel){

        R_xlen_t index = 0;

        StringPiece match;

        if (!all){

            Shield<SEXP>  xs(Rf_allocMatrix(INTSXP, input.size(),2));
            SEXP x = xs;

            string headstr ;

            for(auto it = 0; it!= input.size(); it++){
                size_t headn = 0;
                StringPiece str(R_CHAR(STRING_ELT(inputx, it)));
                auto str_size = strlen(R_CHAR(STRING_ELT(inputx, it)));
                size_t lastIndex = 0;
                if (! ptr->Match(str, lastIndex , str_size, RE2::UNANCHORED, &match, 1)) {
                    INTEGER(x)[it + input.size() *0] = NA_INTEGER;
                    INTEGER(x)[it + input.size() *1] = NA_INTEGER;
                } else {
                    if(match.size()){

                        string mstring = match.as_string();
                        size_t len_mstring = utf8_length(mstring.c_str());
                        string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();

                        size_t len_head = utf8_length(headz.c_str());
                        headn += len_head;

                        INTEGER(x)[it + input.size()*0] = headn+1 ;
                        headn += len_mstring;

                        INTEGER(x)[it + input.size()*1] = headn;
                    } else {

                        string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();
                        size_t len_head = utf8_length(headz.c_str());
                        headn += len_head;

                        INTEGER(x)[it + input.size()*0] =  headn+1 ;

                        INTEGER(x)[it + input.size()*1] =  headn;
                    }

                }
            }
            return x;
        } else { // not parallel ,all
            Shield<SEXP>  xs(Rf_allocVector(VECSXP, input.size()));
            SEXP x = xs;

            Shield<SEXP>  na_matrixx(Rf_allocMatrix(INTSXP, 1,2));
            SEXP na_matrix = na_matrixx;
            INTEGER(na_matrix)[0] = NA_INTEGER;
            INTEGER(na_matrix)[1] = NA_INTEGER;

            string headstr ;
            for(auto it = 0; it!= input.size(); it++){


                size_t headn = 0;
                StringPiece str(R_CHAR(STRING_ELT(inputx, it)));
                auto str_size = strlen(R_CHAR(STRING_ELT(inputx, it)));
                size_t lastIndex = 0;

                vector<tuple<size_t,size_t>> res;

                while (lastIndex < str_size && ptr->Match(str, lastIndex , str_size, RE2::UNANCHORED, &match, 1)){

                    if (match.size()){
                        string mstring = match.as_string();
                        size_t len_mstring = utf8_length(mstring.c_str());

                        string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();

                        size_t len_head = utf8_length(headz.c_str());
                        headn += len_head;

                        size_t head_s = (size_t) headn+1 ;
                        headn += len_mstring;

                        size_t tail_s = (size_t) headn;

                        res.push_back(make_tuple(head_s, tail_s));

                        lastIndex = match.data() - str.data() + match.size();
                    } else {

                        string headz = StringPiece(str.data() + lastIndex, match.data() - str.data() - lastIndex).as_string();
                        size_t len_head = utf8_length(headz.c_str());
                        headn += len_head;
                        res.push_back(make_tuple(headn, headn-1));

                        lastIndex = match.data() - str.data() + match.size();
                        size_t sym_size = getUtf8CharSize(str.data()[lastIndex]);
                        headn+=1;
                        lastIndex += sym_size;
                    }


                }

                if (res.empty()) {
                    SET_VECTOR_ELT(x, index, na_matrix);
                }else{
                    SET_VECTOR_ELT(x, index, Shield<SEXP>(toprotect_loc_matrix(res)));
                }

                index++;
            }
            return x;
        }
    } else{ // parallel
        vector<string> inputv = as<vector<string>>(input);
        if (!all){
            vector<tuple<size_t,size_t>> res(input.size());

            LocateP pobj(inputv, res, ptr);
            parallelFor(0, input.size(), pobj);
            return toprotect_loc_matrix(res);
        } else {
            vector<vector<tuple<size_t,size_t>>> res(input.size());

            LocateAllP pobj(inputv, res, ptr);
            parallelFor(0, input.size(), pobj);

            Shield<SEXP>  xs(Rf_allocVector(VECSXP, input.size()));
            SEXP x = xs;

            R_xlen_t index = 0;

            Shield<SEXP>  na_matrixx(Rf_allocMatrix(INTSXP, 1,2));
            SEXP na_matrix = na_matrixx;
            INTEGER(na_matrix)[0] = NA_INTEGER;
            INTEGER(na_matrix)[1] = NA_INTEGER;

            for (auto resi : res){
                if (resi.empty()) {
                    SET_VECTOR_ELT(x, index, na_matrix);
                }else{
                    SET_VECTOR_ELT(x, index, Shield<SEXP>(toprotect_loc_matrix(resi)));
                }
                index ++;
            }
            return x;
        }

    }
}
