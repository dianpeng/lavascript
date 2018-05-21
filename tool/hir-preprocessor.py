#!/usr/bin/python

import os
import sys
import argparse

""" A preprocessor for all the HIR node

Part of the build init to generate type mapping of HIR for cbase compiler
"""

MARKER     = "LAVA_CBASE_HIR_DEFINE"
ROOT       = "Node"
FILTER     = "node-macro.h"
CLASS      = "HIRTypePredicate"
CLASS2TYPE = "HIRTypeValue"

def _message(x):
    print(x)

def _stdout(x):
    sys.stdout.write(x)

def _stderr(x):
    sys.stderr.write(x)
    sys.stderr.write('\n')

class Node:
    """ represent a parsed node inside of the c++ source code for definition of HIR """
    def __init__(self,name):
        self.name     = name
        self.parent   = None
        self.children = []
        self.meta     = dict()

    def is_leaf(self):
        return len(self.children) == 0

    def is_internal(self):
        return not self.is_leaf()

    def is_base_of(self,name):
        p = self.parent
        while p is not None:
            if p.name == name:
                return True
            p = p.parent
        return False

    def is_child_of(self,name):
        q.extend(self.children)

        while len(q) != 0:
            t = q.pop(0)
            if t.name == name:
                return True
            if t.is_internal():
                q.extend(t.children)

        return False

class Tokenizer:
    TK_SYMBOL = 0
    TK_LPAR   = 1
    TK_RPAR   = 2
    TK_COMMA  = 3
    TK_ASSIGN = 4
    TK_SEMICOLON = 5

    TK_PUBLIC = 11
    TK_PRIVATE= 12
    TK_PROTECT= 13
    TK_TRUE   = 14
    TK_FALSE  = 15
    TK_STRING = 16
    TK_UNKNOWN= 20
    TK_EOF    = 21

    def __init__(self,src,base):
        self.src    = src
        self.pos    = base
        self.symbol = None

    def _skip_ws(self,nc):
        lhs = self.src[self.pos:]
        lstr= lhs.lstrip(nc)
        self.pos = self.pos + (len(lhs) - len(lstr))

    def _sym(self):
        start = self.pos
        while True:
            c = self.src[self.pos:self.pos+1]
            if c.isalnum() or c == '_':
                self.pos = self.pos + 1
            else:
                break
        self.symbol = self.src[start:self.pos]
        return Tokenizer.TK_SYMBOL

    def _str(self):
        self.pos = self.pos+1
        buf = []

        while len(self.src) >= self.pos:
            c = self.src[self.pos:self.pos+1]
            if c == '"':
                self.pos = self.pos+1
                self.symbol = ''.join(buf)
                return Tokenizer.TK_STRING
            elif c == '\\':
                if len(self.src) >= self.pos+1:
                    nc = self.src[self.pos+1:self.pos+2]
                    self.pos = self.pos + 1
                    buf.append(nc)
                else:
                    return Tokenizer.TK_UNKNOWN
            else:
                buf.append(c)

            self.pos = self.pos + 1

        return Tokenizer.TK_UNKNOWN

    def _is_symbol_char(self,c):
        if c.isalnum() or c == '_':
            return True
        return False

    def next(self):
        if self.pos == len(self.src):
            return Tokenizer.TK_EOF

        c = self.src[self.pos:self.pos+1]

        if c == ',':
            self.pos = self.pos + 1
            return Tokenizer.TK_COMMA
        elif c == ';':
            self.pos = self.pos + 1
            return Tokenizer.TK_SEMICOLON
        elif c == '(':
            self.pos = self.pos + 1
            return Tokenizer.TK_LPAR
        elif c == ')':
            self.pos = self.pos + 1
            return Tokenizer.TK_RPAR
        elif c == ' ' or c == '\n':
            self._skip_ws(c)
            return self.next()
        elif c == '=':
            self.pos = self.pos + 1
            return Tokenizer.TK_ASSIGN
        elif c == 'p':
            sstr = self.src[self.pos:]
            if sstr.find("private ") == 0:
                self.pos = self.pos + 7
                return Tokenizer.TK_PRIVATE

            if sstr.find("public ") == 0:
                self.pos = self.pos + 6
                return Tokenizer.TK_PUBLIC

            if sstr.find("protect ") == 0:
                self.pos = self.pos + 7
                return Tokenizer.TK_PROTECT
        elif c == 't':
            sstr = self.src[self.pos:]
            if sstr.find("rue"):
                nc = self.src[self.pos+4:self.pos+5]
                if not self._is_symbol_char(nc):
                    self.pos = self.pos + 4
                    return Tokenizer.TK_TRUE
        elif c == 'f':
            sstr = self.src[self.pos:]
            if sstr.find("alse"):
                nc = self.src[self.pos+5:self.pos+6]
                if not self._is_symbol_char(nc):
                    self.pos = self.pos + 5
                    return Tokenizer.TK_FALSE
        elif c == '"':
            return self._str()

        if c.isalnum() or c == '_':
            return self._sym()

        return Tokenizer.TK_UNKNOWN

class Writer:
    def __init__(self,sinker,indent):
        self.sinker = sinker
        self.indent = indent

    def _indent(self,indent):
        for x in xrange(0,indent):
            self.sinker(self.indent)

    def put(self,msg="",indent=0):
        self._indent(indent)
        self.sinker(msg)

    def put_line(self,msg="",indent=0):
        self.put(msg,indent)
        self.sinker('\n')

class HIRModel:
    def __init__(self):
        self.all_list = dict()

    def _sanity_check(self,src,pos):
        """ sanity check of marker to see whether it is embedded inside of
            comments or not. though embed it inside of comments is undefined
            behavior
        """
        while pos != 0:
            c = src[pos-1:pos]
            if c == '*':
                if pos-1 != 0:
                    pc = src[pos-2:pos-1]
                    if pc == '/':
                        return False
            elif c == '/':
                if pos-1 != 0:
                    pc = src[pos-2:pos-1]
                    if pc == '/':
                        return False
            elif c == '\n':
                break

            pos = pos - 1
        return True

    def _get_node  (self,name):
        n = Node(name)
        if name not in self.all_list:
            self.all_list[name] = n
            return n
        else:
            return self.all_list[name]

    def _parse_meta(self,tokenizer):
        ret = dict()

        ## record the current status of tokenizer
        pos = tokenizer.pos

        ## check whether it actually have a meta field or not,
        ## if it doesn't then just treat it as normal old style
        ## definition , avoiding the need of writing a empty comma
        if tokenizer.next() == Tokenizer.TK_SYMBOL and \
           tokenizer.next() == Tokenizer.TK_COMMA:
            tokenizer.pos = pos
            return ret
        else:
            tokenizer.pos = pos

        tk = tokenizer.next()
        if tk == Tokenizer.TK_COMMA:
            return ret ## empty list

        while True:
            if tk != Tokenizer.TK_SYMBOL:
                return False

            k = tokenizer.symbol

            if tokenizer.next() != Tokenizer.TK_ASSIGN:
                return False

            vtk = tokenizer.next()
            if vtk != Tokenizer.TK_SYMBOL and vtk != Tokenizer.TK_STRING:
                return False

            v = tokenizer.symbol

            ret[k] = v

            tk = tokenizer.next()
            if tk == Tokenizer.TK_COMMA:
                break
            elif tk == Tokenizer.TK_SEMICOLON:
                tk = tokenizer.next()
                if tk == Tokenizer.TK_COMMA:
                    break

        return ret

    def _parse(self,src,start):
        tokenizer = Tokenizer(src,start)
        ## 1. get the marker
        marker = tokenizer.next()
        if marker != Tokenizer.TK_SYMBOL or tokenizer.symbol != MARKER:
            return False
        ## 2. skip the left parenthsis
        if tokenizer.next() != Tokenizer.TK_LPAR:
            return False

        ## 3. meta information list
        meta = self._parse_meta(tokenizer)
        if meta is False:
            return False

        ## 4. find the base
        if tokenizer.next() != Tokenizer.TK_SYMBOL:
            return False
        sub = tokenizer.symbol
        if tokenizer.next() != Tokenizer.TK_COMMA:
            return False

        sub_node = self._get_node(sub)
        sub_node.meta = meta

        specifier = tokenizer.next()
        if specifier != Tokenizer.TK_PUBLIC  and \
           specifier != Tokenizer.TK_PRIVATE and \
           specifier != Tokenizer.TK_PROTECT:
            return False

        base = tokenizer.next()

        if base != Tokenizer.TK_SYMBOL:
            return False

        base_node = self._get_node(tokenizer.symbol)
        base_node.children.append(sub_node)
        sub_node.parent = base_node

        return tokenizer.pos

    def _join(self,cl):
        return ",".join([x.name for x in cl])

    def visit(self,visitor):
        if ROOT in self.all_list:
            root = self.all_list[ROOT]
            q = [root]

            while len(q) != 0:
                x = q.pop(0)
                visitor(x)
                if x.is_internal():
                    q.extend(x.children)

    def dump(self):
        def v(node):
            if node.is_internal():
                print("{0}->{1}".format(node.name,self._join(node.children)))

        self.visit(v)

    def handle_file(self,filename):
        with open(filename,"r") as f:
            data = f.read()
            pos  = 0
            while True:
                marker_pos = data.find(MARKER,pos)
                if marker_pos == -1:
                    break
                else:
                    if not self._sanity_check(data,marker_pos):
                        _stderr("In file {0} the marker {1} seems inside of comment which is forbidden".format(
                            filename,MARKER))
                        return False
                    else:
                        ret = self._parse(data,marker_pos)
                        if ret is False:
                            _stderr("In file {0} failed at parsing marker".format(filename))
                            return False
                        pos = ret


class CxxTypeMapGenerator:
    def __init__(self,writer,model):
        self.writer = writer
        self.model  = model

    def _generate_leaf(self,name):
        node = self.model.all_list[name]
        q = [node]
        ret = set()
        while len(q) != 0:
            x = q.pop(0)
            if x.is_leaf():
                ret.add(x.name)
            else:
                q.extend(x.children)
        return ret

    def _generate_node(self,name):
        # 1. get all reachable leaf from this node
        x = self._generate_leaf(name)
        # 2. generate cxx functions
        self.writer.put_line("// {0}".format(name))
        self.writer.put_line("template<> struct {0}<{1}> {{".format(CLASS,name))
        self.writer.put_line("static bool Test(IRType type) {",1)
        self.writer.put_line("switch(type) {",2)

        for n in x:
            self.writer.put_line("case {0}<{1}>::Value:".format(CLASS2TYPE,n),3)

        self.writer.put_line("return true;",4)
        self.writer.put_line("default:",3)
        self.writer.put_line("return false;",4)
        self.writer.put_line("}",2)
        self.writer.put_line("}",1)
        self.writer.put_line("};",0)
        self.writer.put_line()

    def generate(self):
        """ Generate cxx definition file with regards to all the heirachy """
        def v(node):
            if node.is_internal():
                self._generate_node(node.name)
            else:
                self.writer.put_line("// {0}".format(node.name))
                self.writer.put_line("template<> struct {0}<{1}> {{".format(CLASS,node.name))
                self.writer.put_line("static bool Test(IRType type) {",1)
                self.writer.put_line("return type == {0}<{1}>::Value;".format(CLASS2TYPE,node.name),2)
                self.writer.put_line("}",1)
                self.writer.put_line("};",0)

        self.model.visit(v)

class CxxXMacroGenerator:
    def __init__(self,name,temp,writer,model,base,leaf):
        self.writer = writer
        self.model  = model
        self.name   = name
        self.base   = base
        self.leaf   = leaf
        self.temp   = temp
        self.fmt_kv = None

    def _parse_temp(self):
        """ parsing the template """
        pos = 0
        cnt = []

        while True:
            npos = self.temp.find("{",pos)
            if npos == -1:
                return cnt
            else:
                if len(self.temp) >= npos + 1:
                    nc = self.temp[npos+1:npos+2]
                    if nc == '{':
                        pos = npos + 2
                    else:
                        epos = self.temp.find('}',npos+1)
                        if epos == -1:
                            return False
                        cnt.append( self.temp[npos+1:epos] )
                        pos = epos + 1
                else:
                    return False

        self.fmt_kv = cnt
        return True


    def _predicate_node(self,node):

        if self.leaf and node.is_internal():
            ## we want a leaf but node is not leaf
            return False

        if not self.leaf and node.is_leaf():
            ## we want a internal node but data is leaf
            return False

        if self.base is not None and node.is_child_of(self.base):
            return True

        return False


    def _visit_node(self,node):
        if _predicate_node(node):
            self.writer.put_line(("__({0}) \\".format(self.temp)).format(**node.meta))

    def generate(self):
        """ Generate the xmacro """

        if not self._parse_temp():
            _stderr("template {0} is invalid".format(self.temp))

        cnt = len(self.fmt_kv)

        arglist = []
        for x in xrange(cnt):
            arglist.append("_{0}".format(x))

        self.writer.put_line("#define __({0}) \\".format(",".join(arglist)))

        self.model.visit(self._visit_node)


def _build_model(folder):
    prop = HIRModel()

    itr = next(os.walk(folder))
    root= itr[0]
    fn  = itr[2]

    ## 1. build the HIR model by scanning all the HIR include file
    for filename in fn:
        if filename == FILTER:
            continue
        else:
            _,ext = os.path.splitext(filename)
            if ext != '.h' and ext != '.hh':
                continue

        abspath = os.path.join(root,filename)
        _message("Handle HIR file : {0}".format(abspath))

        prop.handle_file(abspath)

    return prop

def _type_map(model,output):
    ## 2. generate hir type mapping file
    with open(output,"w+") as f:
        def w(msg):
            f.write(msg)

        writer = Writer(w,"  ")
        writer.put_line("// ========================================================================")
        writer.put_line("//   This File Is Generated by hir-preinitory.py ! Do Not Modify !")
        writer.put_line("// ========================================================================")
        writer.put_line()

        writer.put_line("namespace lavascript {")
        writer.put_line("namespace cbase      {")
        writer.put_line("namespace hir        {")

        CxxTypeMapGenerator(writer,model).generate()

        writer.put_line("} // namespace hir")
        writer.put_line("} // namespace cbase")
        writer.put_line("} // namespace lavascript")

def _xmacro(model,name,temp,base,leaf):
    writer = Writer(_message,"  ")
    CxxXMacroGenerator(name,temp,writer,model,base,leaf).generate()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Preinitor for cbase HIR type mapping")
    parser.add_argument("--dir"        ,type=str,action='store'     ,help='specify where the HIR folder is')
    parser.add_argument("--type-map"   ,type=str,action='store'     ,default="",help="node type generation")
    parser.add_argument("--xmacro"     ,type=str,action='store'     ,default="",help="xmacro generation")
    parser.add_argument("--xmacro-temp",type=str,action='store'     ,default="",help="xmacro template"  )
    parser.add_argument("--xmacro-name",type=str,action='store'     ,default="",help="xmacro name")
    parser.add_argument("--xmacro-base",type=str,action='store'     ,default=None,help="xmacro base class")
    parser.add_argument("--xmacro-leaf",action='store_true',default=True,help="xmacro whether leaf node")

    arg = parser.parse_args()

    def bitch_and_die():
        parser.print_help()
        os.exit(-1)

    if arg.dir is None:
        bitch_and_die()

    model = _build_model(arg.dir)

    if arg.type_map is not "":
        _type_map(model,arg.type_map)
    elif arg._xmacro is not "":
        if self.xmacro_temp == "" or self.xmacro_base == "" or self.xmacro_leaf is None or self.xmacro_name == "":
            bitch_and_die()
        _xmacro(model,self.xmacro,self.temp,self.base,self.leaf)
    else:
        parser.print_help()
