/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <graphene/chain/keyword_contract_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <websocketpp/common/md5.hpp>

namespace graphene { namespace chain {
        void_result keyword_contract_evaluator::do_evaluate( const keyword_contract_operation& op )
        { try {

                const database& d = db();

                const account_object& from_account    = op.from(d);
                const account_object& to_account      = op.to(d);
                const asset_object&   asset_type      = op.amount.asset_id(d);

                try {

                    GRAPHENE_ASSERT(
                            is_authorized_asset( d, from_account, asset_type ),
                            transfer_from_account_not_whitelisted,
                            "'from' account ${from} is not whitelisted for asset ${asset}",
                            ("from",op.from)
                                    ("asset",op.amount.asset_id)
                    );
                    GRAPHENE_ASSERT(
                            is_authorized_asset( d, to_account, asset_type ),
                            transfer_to_account_not_whitelisted,
                            "'to' account ${to} is not whitelisted for asset ${asset}",
                            ("to",op.to)
                                    ("asset",op.amount.asset_id)
                    );

                    if( asset_type.is_transfer_restricted() )
                    {
                        GRAPHENE_ASSERT(
                                from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
                                transfer_restricted_transfer_asset,
                                "Asset {asset} has transfer_restricted flag enabled",
                                ("asset", op.amount.asset_id)
                        );
                    }

                    bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= op.amount.amount;
                    FC_ASSERT( insufficient_balance,
                               "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'",
                               ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(op.amount))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

                    return void_result();
                } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

            }  FC_CAPTURE_AND_RETHROW( (op) ) }


        vector<string> split_by_delimeter(const string& str, const char& ch) {
            string next;
            vector<string> result;

            for (string::const_iterator it = str.begin(); it != str.end(); it++) {
                if (*it == ch) {
                    if (!next.empty()) {
                        result.push_back(next);
                        next.clear();
                    }
                } else {
                    next += *it;
                }
            }
            if (!next.empty())
                result.push_back(next);
            return result;
        }


        void_result keyword_contract_evaluator::do_apply( const keyword_contract_operation& o )
        { try {
                const database& d = db();
                const account_object& from_account    = o.from(d);
                const account_object& to_account      = o.to(d);
                memo_data   transaction_memo      =  *o.memo;

                if (to_account.name.find("rgtkeyword") != std::string::npos) {
                    ilog("Step by step");

                    vector<string> hash = split_by_delimeter(to_account.name, '-');
                    std::string hash_for_word = websocketpp::md5::md5_hash_hex(std::string(transaction_memo.message.data(),
                                                                                           transaction_memo.message.size()));

                    if (hash_for_word == hash[2]) {
                        ilog("Ura");
                        db().action_keyword(o.from, o.to, o.amount);
                        db().adjust_balance(o.to, -o.amount);
                        db().adjust_balance(o.from, o.amount);
                    }

                    else {
                        ilog("Tozhe norm");
                        db().adjust_balance( o.from, -o.amount );
                        db().adjust_balance( o.to, o.amount );
                    }
                }
                else {
                    ilog("Regular");
                    db().adjust_balance( o.from, -o.amount );
                    db().adjust_balance( o.to, o.amount );
                }
                return void_result();
            } FC_CAPTURE_AND_RETHROW( (o) ) }

    } } // graphene::chain