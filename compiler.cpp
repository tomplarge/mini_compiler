#include <iostream>
#include <regex>
#include <fstream>
#include <sstream>

using namespace std;

void trim(string &str);

struct Token{
	enum Type {DEF, END, IDENTIFIER, INTEGER, OPAREN, CPAREN, COMMA};

	int type;
	string value;

	Token(int type, string value) {
		this->type = type;
		this->value = value;
	}

	friend ostream& operator<<(ostream& os, const Token &t) {
		return os << "type=" << t.type << ",value=" << t.value << endl;
	}
};

class Tokenizer {
public:
	static const int N = 7;
	const string re[N] = {"^(\\bdef\\b)", "^(\\bend\\b)", "^(\\b[a-zA-Z]+\\b)", "^(\\b[0-9]+\\b)", "^\\(", "^\\)", ","};
	string code;

	Tokenizer(string fname) {
		string snippet;
		ifstream file;

		file.open(fname);
		std::stringstream sstr;
		sstr << file.rdbuf();
		this->code = sstr.str();
		this->code.erase(remove(this->code.begin(), this->code.end(), '\n'), this->code.end());
	}

	deque<Token> tokenize() {
		deque<Token> tokens;

		while (this->code.length()) {
			tokens.push_back(tokenize_one());
		}

		return tokens;
	}

	Token tokenize_one() {
		smatch sm;
		string result;

		for (int t = 0; t < N; t++) {
			if (regex_search(this->code, sm, regex(re[t]))) {
				result = sm.str(0);
				this->code.erase(this->code.begin(), this->code.begin() + result.length());
				trim(this->code);

				return Token(t, result);
			}
		}
	}
};

class Node {
public:
	enum Type {DEFNODE, INTNODE, CALLNODE, VARNODE};

	Node() {}

	virtual void print() = 0;

	virtual int type() { return -2; }
};

class ExprNode : public Node {
public:
	ExprNode() {}

	virtual void print() = 0;

	int type() { return -1; }
};

class DefNode : public Node {
public:
	string name;
	deque<string> args;
	ExprNode* body;

	DefNode(string n, deque<string> a, ExprNode* b) : name(n), args(a), body(b) {}

	int type() { return Node::Type::DEFNODE; }

	void print() {
		cout << "name=" << this->name << ", args=";
		for (auto it = this->args.begin(); it != this->args.end(); cout << *it << ", ", it++) ;
		cout << ", body=";
		this->body->print();
		cout << endl;
	}
};

class IntNode : public ExprNode {
public:
	string value;

	IntNode(string v): value(v) {}

	int type() { return Node::Type::INTNODE; }

	void print() {
		cout << "[(IntNode)value=" << this->value << "]";
	}
};

class CallNode : public ExprNode {
public:
	string name;
	deque<ExprNode*> arg_exprs;

	CallNode(string n, deque<ExprNode*> a) : name(n), arg_exprs(a) { }

	int type() { return Node::Type::CALLNODE; }

	void print() {
		cout << "[(CallNode)name=" << name << ", arg_exprs=";
		for (auto it = arg_exprs.begin(); it != arg_exprs.end(); (*it)->print(), it++) ;
		cout << "]";
	}
};

class VarNode : public ExprNode {
public:
	string name;

	VarNode(string n) : name(n) { }

	int type() { return Node::Type::VARNODE; }

	void print() {
		cout << "[(VarNode)name=" << name << "]";
	}
};

class Parser {
public:
	deque<Token> tokens;

	Parser(deque<Token> tokens) {
		this->tokens = tokens;
	}


	DefNode* parse_def() {
		consume(Token::Type::DEF);

		string name = consume(Token::Type::IDENTIFIER);

		deque<string> arg_names = parse_arg_names();

		ExprNode* body = parse_expr();

		return new DefNode(name, arg_names, body);
	}

	deque<string> parse_arg_names() {
		consume(Token::Type::OPAREN);

		deque<string> arg_names;

		if (peek(Token::Type::IDENTIFIER)) {
			arg_names.push_back(this->tokens.front().value);
			this->tokens.pop_front();

			while(peek(Token::Type::COMMA)) {
				consume(Token::Type::COMMA);

				arg_names.push_back(this->tokens.front().value);
				this->tokens.pop_front();
			}
		}

		consume(Token::Type::CPAREN);

		return arg_names;
	}

	ExprNode* parse_expr() {
		if (peek(Token::Type::INTEGER)) {
			return parse_int();
		} else if (peek(Token::Type::IDENTIFIER) && peek(Token::Type::OPAREN, 1)) {
			return parse_call();
		} else {
			return parse_var_ref();
		}
	}

	IntNode* parse_int() {
		string i = consume(Token::Type::INTEGER);
		return new IntNode(i);
	}

	CallNode* parse_call() {
		string name = consume(Token::Type::IDENTIFIER);
		deque<ExprNode*> arg_exprs = parse_arg_exprs();

		return new CallNode(name, arg_exprs);
	}

	deque<ExprNode*> parse_arg_exprs() {
		consume(Token::Type::OPAREN);

		deque<ExprNode*> arg_exprs;

		if (!peek(Token::Type::CPAREN)) {
			ExprNode* x = parse_expr();

			arg_exprs.push_back(x);

			while (peek(Token::Type::COMMA)) {
				consume(Token::Type::COMMA);
				arg_exprs.push_back(parse_expr());
			}
		}

		consume(Token::Type::CPAREN);

		return arg_exprs;
	}

	VarNode* parse_var_ref() {
		string var = consume(Token::Type::IDENTIFIER);

		return new VarNode(var);
	}

	DefNode* parse() {
		return parse_def();
	}

	string consume(int type) {
		Token token = this->tokens.front();
		this->tokens.pop_front();

		if (token.type == type) {
			return token.value;
		} else {
			char buf[100];
			sprintf(buf, "Could not parse given type %d, expected %d", token.type, type);
			throw runtime_error(buf);
		}
	}

	bool peek(int type, int offset = 0) {
		return this->tokens.at(offset).type == type;
	}
};

class Generator {
public:
	Node* tree;

	Generator(Node* t) : tree(t) {}

	string generate() {
		return generate_helper(this->tree);
	}

	string generate_helper(Node* node) {
		switch(node->type()) {
			case Node::Type::DEFNODE: {
				DefNode* defnode = (DefNode*) node;
				string args = defnode->args.front();
				defnode->args.pop_front();

				for (auto it = defnode->args.begin(); it != defnode->args.end(); it++) {
					args += ", " + *it;
				}

				return "function " + defnode->name + "(" + args + ") {return " + generate_helper(defnode->body) + "};";
			}

			case Node::Type::INTNODE: {
				IntNode* intnode = (IntNode*) node;
				return intnode->value;
			}

			case Node::Type::CALLNODE: {
				CallNode* callnode = (CallNode*) node;
				string arg_exprs = generate_helper(callnode->arg_exprs.front());
				callnode->arg_exprs.pop_front();

				for (auto it = callnode->arg_exprs.begin(); it != callnode->arg_exprs.end(); it++) {
					arg_exprs += ", " + generate_helper(*it);
				}

				return callnode->name + "(" + arg_exprs + ")";
			}

			case Node::Type::VARNODE: {
				VarNode* varnode = (VarNode*) node;
				return varnode->name;
			}

			default:
				char buf[100];
				sprintf(buf, "Could not parse given class type %d", node->type());
				throw runtime_error(buf);
		}
	}
};


void trim(string& str) {
	auto it = str.begin();
	for (it; it != str.end() && *it == ' '; it++) ;
	str.erase(str.begin(), it);
}

int main() {
	Tokenizer t = Tokenizer("test.lang");
	deque<Token> tokens = t.tokenize();

	Parser parser = Parser(tokens);
	DefNode* tree = parser.parse();

	// tree->print();

	Generator generator = Generator(tree);
	string generated = generator.generate();

	string RUNTIME = "function add(x,y) { return x+y };";
	string TEST = "console.log(f(1,2));";

	cout << RUNTIME << "\n" << generated << "\n" << TEST << endl;

	return 0;
}
