
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <boost/bind.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_grammar.hpp>
#include <boost/optional.hpp>

#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>

#include <osmium/osm.hpp>

#include "object_filter.hpp"


namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator>
struct comment_skipper : public qi::grammar<Iterator> {

    qi::rule<Iterator> skip;

    comment_skipper() : comment_skipper::base_type(skip, "Comment skipper") {
        skip = ascii::space
             | ('#' >> *(qi::char_ - '\n') >> '\n');
    }

};

ExprNode* boolean_and(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
    auto expressions = boost::fusion::at_c<0>(e);
    assert(!expressions.empty());
    if (expressions.size() == 1) {
        return expressions[0];
    } else {
        return new AndExpr(expressions);
    }
}

ExprNode* boolean_or(const boost::fusion::vector<std::vector<ExprNode*>>& e) {
    auto expressions = boost::fusion::at_c<0>(e);
    assert(!expressions.empty());
    if (expressions.size() == 1) {
        return expressions[0];
    } else {
        return new OrExpr(expressions);
    }
}

ExprNode* boolean_not(const boost::fusion::vector<ExprNode*>& e) {
    auto expression = boost::fusion::at_c<0>(e);
    return new NotExpr(expression);
}

ExprNode* check_has_key_expr(const boost::fusion::vector<std::string>& e) {
    auto key = boost::fusion::at_c<0>(e);
    return new CheckHasKeyExpr(key);
}

ExprNode* check_tag_str_expr(const boost::fusion::vector<std::tuple<std::string, std::string, std::string>>& e) {
    auto key   = std::get<0>(boost::fusion::at_c<0>(e));
    auto oper  = std::get<1>(boost::fusion::at_c<0>(e));
    auto value = std::get<2>(boost::fusion::at_c<0>(e));
    return new CheckTagStrExpr(key, oper, value);
}

ExprNode* check_tag_regex_expr(const boost::fusion::vector<std::tuple<std::string, std::string, std::string, boost::optional<char>>>& e) {
    auto key   = std::get<0>(boost::fusion::at_c<0>(e));
    auto oper  = std::get<1>(boost::fusion::at_c<0>(e));
    auto value = std::get<2>(boost::fusion::at_c<0>(e));
    auto cins  = std::get<3>(boost::fusion::at_c<0>(e));
    return new CheckTagRegexExpr(key, oper, value, cins);
}

ExprNode* check_object_type_expr(const boost::fusion::vector<osmium::item_type>& e) {
    const auto type = boost::fusion::at_c<0>(e);
    return new CheckObjectTypeExpr(type);
}

ExprNode* int_attr_expr(const boost::fusion::vector<integer_attribute_type>& e) {
    auto attr = boost::fusion::at_c<0>(e);
    return new IntegerAttribute{attr};
}

ExprNode* str_attr_expr(const boost::fusion::vector<string_attribute_type>& e) {
    auto attr = boost::fusion::at_c<0>(e);
    return new StringAttribute{attr};
}

ExprNode* make_tags_expr(const boost::fusion::vector<ExprNode*>& e) {
    auto expr = boost::fusion::at_c<0>(e);
    return new TagsExpr{expr};
}

ExprNode* make_nodes_expr(const boost::fusion::vector<ExprNode*>& e) {
    auto expr = boost::fusion::at_c<0>(e);
    return new NodesExpr{expr};
}

ExprNode* make_members_expr(const boost::fusion::vector<ExprNode*>& e) {
    auto expr = boost::fusion::at_c<0>(e);
    return new MembersExpr{expr};
}

ExprNode* binary_int_op_expr(const boost::fusion::vector<std::tuple<ExprNode*, integer_op_type, ExprNode*>>& e) {
    auto e1 = std::get<0>(boost::fusion::at_c<0>(e));
    auto op = std::get<1>(boost::fusion::at_c<0>(e));
    auto e2 = std::get<2>(boost::fusion::at_c<0>(e));
    return new BinaryIntOperation{e1, op, e2};
}

ExprNode* binary_str_op_expr(const boost::fusion::vector<std::tuple<ExprNode*, std::tuple<string_op_type, ExprNode*>>>& e) {
    auto e1 = std::get<0>(boost::fusion::at_c<0>(e));
    auto op = std::get<0>(std::get<1>(boost::fusion::at_c<0>(e)));
    auto e2 = std::get<1>(std::get<1>(boost::fusion::at_c<0>(e)));
    return new BinaryStrOperation{e1, op, e2};
}

ExprNode* int_value_expr(const boost::fusion::vector<std::int64_t>& e) {
    auto value = boost::fusion::at_c<0>(e);
    return new IntegerValue{value};
}

ExprNode* str_value_expr(const boost::fusion::vector<std::string>& e) {
    auto value = boost::fusion::at_c<0>(e);
    return new StringValue{value};
}

ExprNode* regex_value_expr(const boost::fusion::vector<std::string>& e) {
    auto value = boost::fusion::at_c<0>(e);
    return new RegexValue{value};
}

template <typename Iterator>
struct OSMObjectFilterGrammar : qi::grammar<Iterator, comment_skipper<Iterator>, ExprNode*()> {

    template <typename... T>
    using rs = qi::rule<Iterator, comment_skipper<Iterator>, T...>;

    rs<ExprNode*()> expression, paren_expression, factor, tag, primitive, key, attr, term, int_value, attr_int, str_value, regex_value, attr_str;
    rs<ExprNode*()> subexpression, tags_expr, nodes_expr, members_expr, subexpr_int;
    rs<std::string()> single_q_str, double_q_str, plain_string, string, oper_str_old, oper_regex_old;
    rs<osmium::item_type()> object_type, attr_type;
    rs<integer_op_type> oper_int;
    rs<string_op_type> oper_str;
    rs<string_op_type> oper_regex;
    rs<std::tuple<std::string, std::string, std::string>()> key_oper_str_value;
    rs<std::tuple<std::string, std::string, std::string, boost::optional<char>>()> key_oper_regex_value;
    rs<std::tuple<ExprNode*, integer_op_type, ExprNode*>()> binary_int_oper;
    rs<std::tuple<ExprNode*, std::tuple<string_op_type, ExprNode*>>()> binary_str_oper;
    rs<std::tuple<string_op_type, ExprNode*>()> binary_str_oper_str, binary_str_oper_regex;

    OSMObjectFilterGrammar() :
        OSMObjectFilterGrammar::base_type(expression, "OSM Object Filter Grammar") {

        // single quoted string
        single_q_str   = qi::lit('\'')
                       > *(~qi::char_('\''))
                       > qi::lit('\'');
        single_q_str.name("single quoted string");

        // double quoted string (XXX TODO: escapes, unicode)
        double_q_str   = qi::lit('"')
                       > *(~qi::char_('"'))
                       > qi::lit('"');
        double_q_str.name("double quoted string");

        // plain string as used in keys and values
        plain_string   =   qi::char_("a-zA-Z")
                       >> *qi::char_("a-zA-Z0-9:_");
        plain_string.name("plain string");

        // any kind of string
        string         = plain_string
                       | single_q_str
                       | double_q_str;
        string.name("string");

        // operator for integer comparison
        oper_int       = (qi::lit("=")  > qi::attr(integer_op_type::equal))
                       | (qi::lit("!=") > qi::attr(integer_op_type::not_equal))
                       | (qi::lit("<=") > qi::attr(integer_op_type::less_or_equal))
                       | (qi::lit("<")  > qi::attr(integer_op_type::less_than))
                       | (qi::lit(">=") > qi::attr(integer_op_type::greater_or_equal))
                       | (qi::lit(">")  > qi::attr(integer_op_type::greater_than));
        oper_int.name("integer comparison operand");

        // operator for simple string comparison
        oper_str_old       = ascii::string("=")
                       | ascii::string("!=");
        oper_str_old.name("string comparison operand");

        // operator for simple string comparison
        oper_str       = (qi::lit("=")  > qi::attr(string_op_type::equal))
                       | (qi::lit("!=") > qi::attr(string_op_type::not_equal));
        oper_str.name("string comparison operand");

        // operator for regex string comparison
        oper_regex     = (qi::lit("~")  > qi::attr(string_op_type::match))
                       | (qi::lit("!~") > qi::attr(string_op_type::not_match));
        oper_regex.name("string regex comparison operand");

        // operator for regex string comparison
        oper_regex_old     = ascii::string("~")
                       | ascii::string("=~")
                       | ascii::string("!~");
        oper_regex_old.name("string comparison operand");

        // a tag key
        key            = string[qi::_val = boost::phoenix::bind(&check_has_key_expr, _1)];
        key.name("tag key");

        // a tag (key operator value)
        key_oper_str_value =  string
                           >> oper_str_old
                           >> string;
        key_oper_str_value.name("key_oper_str_value");

        key_oper_regex_value =  string
                             >> oper_regex_old
                             >> string
                             >> -ascii::char_('i');
        key_oper_regex_value.name("key_oper_regex_value");

        // a tag
        tag            = key_oper_str_value[qi::_val = boost::phoenix::bind(&check_tag_str_expr, _1)]
                       | key_oper_regex_value[qi::_val = boost::phoenix::bind(&check_tag_regex_expr, _1)];
        tag.name("tag");

        attr_int       = ((qi::lit("@id")        > qi::attr(integer_attribute_type::id))
                         |(qi::lit("@version")   > qi::attr(integer_attribute_type::version))
                         |(qi::lit("@uid")       > qi::attr(integer_attribute_type::uid))
                         |(qi::lit("@changeset") > qi::attr(integer_attribute_type::changeset))
                         |(qi::lit("@ref")       > qi::attr(integer_attribute_type::ref))
                         )[qi::_val = boost::phoenix::bind(&int_attr_expr, _1)];
        attr_int.name("integer attribute");

        attr_str       = ((qi::lit("@user")  > qi::attr(string_attribute_type::user))
                         |(qi::lit("@key")   > qi::attr(string_attribute_type::key))
                         |(qi::lit("@value") > qi::attr(string_attribute_type::value))
                         |(qi::lit("@role")  > qi::attr(string_attribute_type::role))
                         )[qi::_val = boost::phoenix::bind(&str_attr_expr, _1)];
        attr_str.name("string attribute");

        subexpression  = qi::lit('[') > expression > qi::lit(']');
        subexpression.name("tags expression");

        tags_expr      = qi::lit("@tags") >> subexpression;
        tags_expr.name("tags expression");

        nodes_expr     = qi::lit("@nodes") >> subexpression;
        nodes_expr.name("nodes expression");

        members_expr   = qi::lit("@members") >> subexpression;
        members_expr.name("members expression");

        //int_value      = qi::long_[qi::_val = boost::phoenix::bind(&int_value_expr, _1)];
        int_value      = qi::int_parser<std::int64_t>()[qi::_val = boost::phoenix::bind(&int_value_expr, _1)];
        int_value.name("integer value");

        str_value      = string[qi::_val = boost::phoenix::bind(&str_value_expr, _1)];
        str_value.name("string value");

        regex_value      = string[qi::_val = boost::phoenix::bind(&regex_value_expr, _1)];
        regex_value.name("regex value");

        subexpr_int    = tags_expr[qi::_val = boost::phoenix::bind(&make_tags_expr, _1)]
                       | nodes_expr[qi::_val = boost::phoenix::bind(&make_nodes_expr, _1)]
                       | members_expr[qi::_val = boost::phoenix::bind(&make_members_expr, _1)];

        // an attribute name, comparison operator and integer
        binary_int_oper  = (attr_int | int_value | subexpr_int)
                         >> oper_int
                         >> (attr_int | int_value | subexpr_int);
        binary_int_oper.name("binary_int_oper");

        binary_str_oper_str = oper_str > str_value;
        binary_str_oper_regex = oper_regex > regex_value;

        binary_str_oper  = attr_str >
                           (binary_str_oper_str |binary_str_oper_regex);
        binary_str_oper.name("binary_str_oper");

        // name of OSM object type
        object_type    = (qi::lit("node")     > qi::attr(osmium::item_type::node))
                       | (qi::lit("way")      > qi::attr(osmium::item_type::way))
                       | (qi::lit("relation") > qi::attr(osmium::item_type::relation));
        object_type.name("object type");

        // OSM object type as attribute
        attr_type      = qi::lit("@type")
                       > '='
                       > object_type;
        attr_type.name("attr_type");

        // attribute expression
        attr           = attr_type[qi::_val = boost::phoenix::bind(&check_object_type_expr, _1)]
                       | binary_int_oper[qi::_val = boost::phoenix::bind(&binary_int_op_expr, _1)]
                       | binary_str_oper[qi::_val = boost::phoenix::bind(&binary_str_op_expr, _1)];
        attr.name("attr");

        // primitive expression
        primitive      = object_type[qi::_val = boost::phoenix::bind(&check_object_type_expr, _1)]
                       | tag[qi::_val = qi::_1]
                       | key[qi::_val = qi::_1]
                       | attr[qi::_val = qi::_1];
        primitive.name("condition");

        // boolean logic expressions
        expression      = (term % qi::lit("or"))[qi::_val = boost::phoenix::bind(&boolean_or, _1)];
        expression.name("expression");

        term            = (factor % qi::lit("and"))[qi::_val = boost::phoenix::bind(&boolean_and, _1)];
        term.name("term");

        paren_expression = '('
                         > expression[qi::_val = qi::_1]
                         > ')';
        paren_expression.name("parenthesized expression");

        factor          = (qi::lit("not") >> factor)[qi::_val = boost::phoenix::bind(&boolean_not, _1)]
                        | paren_expression[qi::_val = qi::_1]
                        | primitive[qi::_val = qi::_1];
        factor.name("factor");

        qi::on_error<qi::fail>(expression,
            std::cerr
                << boost::phoenix::val("ERROR: Expecting ")
                << boost::spirit::_4
                << boost::phoenix::val(" here: \"")
                << boost::phoenix::construct<std::string>(boost::spirit::_3, boost::spirit::_2)
                << boost::phoenix::val("\"\n")
        );

    }

}; // struct OSMObjectFilterGrammar

OSMObjectFilter::OSMObjectFilter(std::string& input) {
    comment_skipper<std::string::iterator> skip;

    OSMObjectFilterGrammar<std::string::iterator> grammar;

    auto first = input.begin();
    auto last = input.end();
    const bool result = qi::phrase_parse(
        first,
        last,
        grammar,
        skip,
        m_root
    );

    if (!result || first != last) {
        throw std::runtime_error{"Can not parse expression"};
    }

    assert(m_root);
}

