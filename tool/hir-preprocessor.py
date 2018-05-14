#!/usr/bin/python

import os
import sys
import argparse

""" A preprocessor for all the HIR node

Part of the build process to generate type mapping of HIR for cbase compiler
"""

MARKER = "LAVA_CBASE_HIR_DEFINE"
ROOT   = "Node"
FILTER = "node-macro.h"
CLASS  = "HIRTypePredicate"
CLASS2TYPE = "HIRTypeValue"

class Node:
    def __init__(self,name):
        self.name = name
        self.children = []

    def is_leaf(self):
        return len(self.children) == 0

    def is_internal(self):
        return not self.is_leaf()

class Tokenizer:
    TK_SYMBOL = 0
    TK_LPAR   = 1
    TK_RPAR   = 2
    TK_COMMA  = 3

    TK_PUBLIC = 4
    TK_PRIVATE= 5
    TK_PROTECT= 6
    TK_UNKNOWN= 7
    TK_EOF    = 8

    def __init__(self,src,base):
        self.src    = src
        self.base   = base
        self.pos    = base
        self.symbol = None

    def _skip_ws(self):
        lhs = self.src[self.pos:]
        lstr= lhs.lstrip()
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

    def next(self):
        if self.pos == len(self.src):
            return Tokenizer.TK_EOF

        c = self.src[self.pos:self.pos+1]

        if c == ',':
            self.pos = self.pos + 1
            return Tokenizer.TK_COMMA
        elif c == '(':
            self.pos = self.pos + 1
            return Tokenizer.TK_LPAR
        elif c == ')':
            self.pos = self.pos + 1
            return Tokenizer.TK_RPAR
        elif c == ' ':
            self.pos = self.pos + 1
            self._skip_ws()
            return self.next()
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

class Processor:
    def __init__(self):
        self.all_list = dict()

    def _has_marker(self,line):
        pos = line.find(MARKER)
        if pos == -1:
            return -1
        else:
            cmt_pos = line.find("//")
            if cmt_pos != -1:
                if cmt_pos < pos:
                    return -1

            cmt_pos = line.find("/*")
            if cmt_pos != -1:
                if cmt_pos < pos:
                    return -1

            return pos

    def _get_node  (self,name):
        n = Node(name)
        if name not in self.all_list:
            self.all_list[name] = n
            return n
        else:
            return self.all_list[name]

    def _parse_line(self,start,line):
        tokenizer = Tokenizer(line,start)
        ## 1. get the marker
        marker = tokenizer.next()
        if marker != Tokenizer.TK_SYMBOL or tokenizer.symbol != MARKER:
            print(1)
            return False
        ## 2. skip the left parenthsis
        if tokenizer.next() != Tokenizer.TK_LPAR:
            print(2)
            return False
        ## 3. find the base
        if tokenizer.next() != Tokenizer.TK_SYMBOL:
            return False
        sub = tokenizer.symbol
        if tokenizer.next() != Tokenizer.TK_COMMA:
            return False
        sub_node = self._get_node(sub)

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

        return True

    def _join(self,cl):
        return ",".join([x.name for x in cl])

    def _visit(self,visitor):
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

        self._visit(v)

    def _generate_leaf(self,name):
        node = self.all_list[name]
        q = [node]
        ret = set()
        while len(q) != 0:
            x = q.pop(0)
            if x.is_leaf():
                ret.add(x.name)
            else:
                q.extend(x.children)
        return ret

    def _generate_node(self,name,writer):
        # 1. get all reachable leaf from this node
        x = self._generate_leaf(name)
        # 2. generate cxx functions
        writer.put_line("// {0}".format(name))
        writer.put_line("template<> struct {0}<{1}> {{".format(CLASS,name))
        writer.put_line("static bool Test(IRType type) {",1)
        writer.put_line("switch(type) {",2)

        for n in x:
            writer.put_line("case {0}<{1}>::Value:".format(CLASS2TYPE,n),3)

        writer.put_line("return true;",4)
        writer.put_line("default:",3)
        writer.put_line("return false;",4)
        writer.put_line("}",2)
        writer.put_line("}",1)
        writer.put_line("};",0)
        writer.put_line()

    def generate_cxx(self,writer):
        """ Generate cxx definition file with regards to all the heirachy """
        def v(node):
            if node.is_internal():
                self._generate_node(node.name,writer)
            else:
                writer.put_line("// {0}".format(node.name))
                writer.put_line("template<> struct {0}<{1}> {{".format(CLASS,node.name))
                writer.put_line("static bool Test(IRType type) {",1)
                writer.put_line("return type == {0}<{1}>::Value;".format(CLASS2TYPE,node.name),2)
                writer.put_line("}",1)
                writer.put_line("};",0)
        self._visit(v)

    def process(self,filename):
        with open(filename,"r") as f:
            for line in f:
                pos = self._has_marker(line)
                if pos != -1:
                    self._parse_line(pos,line)

def _message(x):
    print(x)

def _stdout(x):
    sys.stdout.write(x)

def _scan(folder,output):
    prop = Processor()

    itr = next(os.walk(folder))
    root= itr[0]
    fn  = itr[2]

    for filename in fn:
        if filename == FILTER:
            continue
        abspath = os.path.join(root,filename)
        _message("Handle HIR file : {0}".format(abspath))
        prop.process(abspath)

    with open(output,"w+") as f:
        def w(msg):
            f.write(msg)
        writer = Writer(w,"  ")
        writer.put_line("// ========================================================================")
        writer.put_line("//   This File Is Generated by hir-preprocessory.py ! Do Not Modify !")
        writer.put_line("// ========================================================================")
        writer.put_line()

        writer.put_line("namespace lavascript {")
        writer.put_line("namespace cbase      {")
        writer.put_line("namespace hir        {")

        prop.generate_cxx(writer)

        writer.put_line("} // namespace hir")
        writer.put_line("} // namespace cbase")
        writer.put_line("} // namespace lavascript")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Preprocessor for cbase HIR type mapping")
    parser.add_argument("--dir",type=str,action='store',help='specify where the HIR folder is')
    parser.add_argument("--output",type=str,action='store',help='specify where the generated file path')
    arg = parser.parse_args()
    if arg.dir is not None and arg.output is not None:
        _scan(arg.dir,arg.output)
    else:
        parser.print_help()
