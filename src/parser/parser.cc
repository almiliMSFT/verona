#include "parser.h"

#include "../ast/path.h"
#include "lexer.h"
#include "source.h"

#include <deque>

namespace verona::parser
{
  enum Result
  {
    Skip,
    Success,
    Error,
  };

  struct Parse
  {
    Source source;
    size_t pos;
    std::deque<Token> lookahead;
    size_t la;
    err::Errors& err;

    Parse(Source& source, err::Errors& err)
    : source(source), pos(0), la(0), err(err)
    {}

    Result peek(TokenKind kind)
    {
      if (la >= lookahead.size())
        lookahead.push_back(lex(source, pos));

      assert(la < lookahead.size());

      if (lookahead[la].kind == kind)
      {
        la++;
        return Success;
      }

      return Error;
    }

    Token take()
    {
      assert(la == 0);

      if (lookahead.size() == 0)
        return lex(source, pos);

      auto tok = lookahead.front();
      lookahead.pop_front();
      return tok;
    }

    void rewind()
    {
      la = 0;
    }

    Result has(TokenKind kind)
    {
      assert(la == 0);

      if (peek(kind))
      {
        rewind();
        take();
        return Success;
      }

      return Error;
    }

    Result ident(ID& id)
    {
      if (peek(TokenKind::Ident) == Error)
      {
        err << "Expected identifier" << err::end;
        return Error;
      }

      rewind();
      id = take().location;
      return Success;
    }

    // TODO: while, for, match, try, atom sequence
    // TODO: storing a location for all ast nodes?

    Result atoms(Node<Expr>& expr)
    {
      // TODO: atoms
      return Success;
    }

    Result when(Node<Expr>& expr)
    {
      // when <- `when` tuple block
      if (has(TokenKind::When) != Success)
        return Skip;

      auto when = std::make_shared<When>();
      expr = when;

      if (tuple(when->waitfor) != Success)
      {
        err << "Expected a tuple" << err::end;
        return Error;
      }

      if (block(when->behaviour) != Success)
      {
        err << "Expected a block" << err::end;
        return Error;
      }

      return Success;
    }

    Result conditional(Node<Expr>& expr)
    {
      // if <- `if` tuple block (`else` block)?
      if (has(TokenKind::If) != Success)
        return Skip;

      auto cond = std::make_shared<Conditional>();
      expr = cond;

      if (tuple(cond->cond) != Success)
      {
        err << "Expected a condition" << err::end;
        return Error;
      }

      if (block(cond->on_true) != Success)
      {
        err << "Expected a block" << err::end;
        return Error;
      }

      if (has(TokenKind::Else) != Success)
        return Success;

      if (block(cond->on_false) != Success)
      {
        err << "Expected a block" << err::end;
        return Error;
      }

      return Success;
    }

    Result tuple(Node<Tuple>& expr)
    {
      // tuple <- `(` expr* `)`
      if (has(TokenKind::LParen) != Success)
      {
        err << "Expected (" << err::end;
        return Error;
      }

      if (has(TokenKind::RParen) == Success)
        return Success;

      do
      {
        expr->seq.push_back({});

        if (expression(expr->seq.back()) == Error)
          return Error;
      } while (has(TokenKind::Comma) == Success);

      if (has(TokenKind::RParen) != Success)
      {
        err << "Expected , or )" << err::end;
        return Error;
      }

      return Success;
    }

    Result opttuple(Node<Expr>& expr)
    {
      if (peek(TokenKind::LParen) != Success)
        return Skip;

      rewind();
      auto tup = std::make_shared<Tuple>();
      expr = tup;
      return tuple(tup);
    }

    Result block(Node<Block>& expr)
    {
      // block <- `{` expr* `}`
      if (has(TokenKind::LBrace) != Success)
      {
        err << "Expected {" << err::end;
        return Error;
      }

      while (has(TokenKind::RBrace) != Success)
      {
        // TODO:
      }

      return Success;
    }

    Result optblock(Node<Expr>& expr)
    {
      if (peek(TokenKind::LBrace) != Success)
        return Skip;

      rewind();
      auto blk = std::make_shared<Block>();
      expr = blk;
      return block(blk);
    }

    Result expression(Node<Expr>& expr)
    {
      // TODO:
      return Success;
    }

    Result initexpr(Node<Expr>& expr)
    {
      if (has(TokenKind::Equals) != Success)
        return Skip;

      return expression(expr);
    }

    Result typeexpr(Node<Type>& type)
    {
      // TODO:
      return Success;
    }

    Result inittype(Node<Type>& type)
    {
      if (has(TokenKind::Equals) != Success)
        return Skip;

      return typeexpr(type);
    }

    Result oftype(Node<Type>& type)
    {
      if (has(TokenKind::Colon) != Success)
        return Skip;

      return typeexpr(type);
    }

    Result parameter(Param& param)
    {
      if (ident(param.id) == Error)
        return Error;

      if (oftype(param.type) == Error)
        return Error;

      if (initexpr(param.init) == Error)
        return Error;

      return Success;
    }

    Result parameters(List<Param>& params)
    {
      if (has(TokenKind::LParen) != Success)
      {
        err << "Expected (" << err::end;
        return Error;
      }

      if (has(TokenKind::RParen) == Success)
        return Success;

      do
      {
        auto param = std::make_shared<Param>();
        params.push_back(param);

        if (parameter(*param) == Error)
          return Error;
      } while (has(TokenKind::Comma) == Success);

      if (has(TokenKind::RParen) != Success)
      {
        err << "Expected , or )" << err::end;
        return Error;
      }

      return Success;
    }

    Result signature(Node<Signature>& sig)
    {
      sig = std::make_shared<Signature>();

      if (typeparams(sig->typeparams) == Error)
        return Error;

      if (parameters(sig->params) == Error)
        return Error;

      if (oftype(sig->result) == Error)
        return Error;

      if (has(TokenKind::Throws))
      {
        if (typeexpr(sig->throws) == Error)
          return Error;
      }

      if (constraints(sig->constraints) == Error)
        return Error;

      return Success;
    }

    Result field(List<Member>& members)
    {
      auto field = std::make_shared<Field>();

      if (ident(field->id) == Error)
        return Error;

      if (oftype(field->type) == Error)
        return Error;

      if (initexpr(field->init) == Error)
        return Error;

      if (has(TokenKind::Semicolon) != Success)
      {
        err << "Expected ;" << err::end;
        return Error;
      }

      return Success;
    }

    Result function(Function& func)
    {
      if (peek(TokenKind::Ident))
      {
        rewind();
        func.id = take().location;
      }
      else if (peek(TokenKind::Symbol))
      {
        rewind();
        func.id = take().location;
      }

      if (signature(func.signature) == Error)
        return Error;

      if (block(func.body) == Success)
        return Success;

      return has(TokenKind::Semicolon);
    }

    Result static_function(List<Member>& members)
    {
      auto func = std::make_shared<Function>();
      members.push_back(func);
      return function(*func);
    }

    Result method(List<Member>& members)
    {
      auto method = std::make_shared<Method>();
      members.push_back(method);
      return function(*method);
    }

    Result field_or_function(List<Member>& members)
    {
      if (has(TokenKind::Static) != Success)
      {
        // It's a static function.
        return static_function(members);
      }

      // field <- id oftype? initexpr? `;`
      // method <- (id / sym)? sig (block / `;`)
      // sig <- typeparams? params oftype? constraints?
      if (peek(TokenKind::Ident))
      {
        if (peek(TokenKind::LSquare) || peek(TokenKind::LParen))
        {
          // It's a method.
          rewind();
          method(members);
        }
        else
        {
          // It's a field.
          rewind();
          field(members);
        }
      }
      else if (
        peek(TokenKind::Symbol) || peek(TokenKind::LSquare) ||
        peek(TokenKind::LParen))
      {
        // It's a method.
        rewind();
        method(members);
      }

      // It's not a field, function, or method.
      return Skip;
    }

    Result constraints(List<Constraint>& constraints)
    {
      while (has(TokenKind::Where) == Success)
      {
        auto constraint = std::make_shared<Constraint>();

        if (ident(constraint->id) == Error)
          return Error;

        if (oftype(constraint->type) == Error)
          return Error;

        if (inittype(constraint->init) == Error)
          return Error;

        constraints.push_back(constraint);
      }

      return Success;
    }

    Result typeparams(std::vector<ID>& typeparams)
    {
      if (has(TokenKind::LSquare) != Success)
        return Skip;

      do
      {
        ID id;

        if (ident(id) == Error)
          return Error;

        typeparams.push_back(id);
      } while (has(TokenKind::Comma) == Success);

      if (has(TokenKind::RSquare) != Success)
      {
        err << "Expected , or ]" << err::end;
        return Error;
      }

      return Success;
    }

    Result entity(Entity& ent)
    {
      if (typeparams(ent.typeparams) == Error)
        return Error;

      if (oftype(ent.inherits) == Error)
        return Error;

      if (constraints(ent.constraints) == Error)
        return Error;

      return Success;
    }

    Result namedentity(NamedEntity& ent)
    {
      if (ident(ent.id) == Error)
        return Error;

      if (entity(ent) == Error)
        return Error;

      return Success;
    }

    Result typealias(List<Member>& members)
    {
      if (has(TokenKind::Type) != Success)
        return Skip;

      auto alias = std::make_shared<TypeAlias>();

      if (namedentity(*alias) == Error)
        return Error;

      if (has(TokenKind::Equals) != Success)
      {
        err << "Expected =" << err::end;
        return Error;
      }

      if (typeexpr(alias->type) == Error)
        return Error;

      if (has(TokenKind::Semicolon) != Success)
      {
        err << "Expected ;" << err::end;
        return Error;
      }

      members.push_back(alias);
      return Success;
    }

    Result interface(List<Member>& members)
    {
      if (has(TokenKind::Interface) != Success)
        return Skip;

      auto iface = std::make_shared<Interface>();

      if (namedentity(*iface) == Error)
        return Error;

      if (typebody(iface->members) == Error)
        return Error;

      members.push_back(iface);
      return Success;
    }

    Result classdef(List<Member>& members)
    {
      if (has(TokenKind::Class) != Success)
        return Skip;

      auto cls = std::make_shared<Class>();

      if (namedentity(*cls) == Error)
        return Error;

      if (typebody(cls->members) == Error)
        return Error;

      members.push_back(cls);
      return Success;
    }

    Result moduledef(List<Member>& members)
    {
      if (has(TokenKind::Module) != Success)
        return Skip;

      auto module = std::make_shared<Module>();

      if (entity(*module) == Error)
        return Error;

      if (has(TokenKind::Semicolon) != Success)
      {
        err << "Expected ;" << err::end;
        return Error;
      }

      members.push_back(module);
      return Success;
    }

    Result member(List<Member>& members, bool printerr)
    {
      Result r;

      if ((r = moduledef(members)) != Skip)
        return r;

      if ((r = classdef(members)) != Skip)
        return r;

      if ((r = interface(members)) != Skip)
        return r;

      if ((r = typealias(members)) != Skip)
        return r;

      if ((r = field_or_function(members)) != Skip)
        return r;

      if (printerr)
      {
        err << "Expected a module, class, interface, type alias, field, or "
               "function"
            << err::end;
      }

      return Error;
    }

    Result typebody(List<Member>& members)
    {
      if (has(TokenKind::LBrace) != Success)
      {
        err << "Expected {" << err::end;
        return Error;
      }

      auto result = Success;
      auto printerr = true;

      while (has(TokenKind::RBrace) != Success)
      {
        if (has(TokenKind::End) == Success)
        {
          err << "Expected }" << err::end;
          return Error;
        }

        if (member(members, printerr) == Error)
        {
          printerr = false;
          result = Error;
          take();
        }
        else
        {
          printerr = true;
        }
      }

      return Success;
    }

    Result module(List<Member>& members)
    {
      auto result = Success;
      auto printerr = true;

      while (has(TokenKind::End) != Success)
      {
        if (member(members, printerr) == Error)
        {
          printerr = false;
          result = Error;
          take();
        }
        else
        {
          printerr = true;
        }
      }

      return result;
    }
  };

  Result
  parse_file(const std::string& file, Node<Class>& module, err::Errors& err)
  {
    auto source = load_source(file, err);

    if (!source)
      return Error;

    Parse parse(source, err);
    return parse.module(module->members);
  }

  Result parse_directory(
    const std::string& path, Node<Class>& module, err::Errors& err)
  {
    constexpr auto ext = "verona";
    auto files = path::files(path);
    auto result = Success;

    if (files.empty())
    {
      err << "No " << ext << " files found in " << path << err::end;
      return Error;
    }

    for (auto& file : files)
    {
      if (ext != path::extension(file))
        continue;

      auto filename = path::join(path, file);

      if (parse_file(filename, module, err) == Error)
        result = Error;
    }

    return result;
  }

  Node<NodeDef> parse(const std::string& path, err::Errors& err)
  {
    auto module = std::make_shared<Class>();

    if (path::is_directory(path))
      parse_directory(path, module, err);
    else
      parse_file(path, module, err);

    return module;
  }
}