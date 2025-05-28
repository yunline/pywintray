import ast
import copy
import importlib.util
import os
import typing

def load_pyi_ast(lib_name:str) -> ast.Module:
    spec = importlib.util.find_spec(lib_name)
    if spec is None or spec.origin is None:
        raise RuntimeError("lib not found")
    dir_name = os.path.dirname(spec.origin)
    pyi_name = os.path.basename(spec.origin).split(".")[0]+".pyi"
    pyi_path = os.path.join(dir_name, pyi_name)
    with open(pyi_path, "r", encoding="utf8") as pyi_file:
        return ast.parse(pyi_file.read(), filename=pyi_path)

def get_named_node(namespace:ast.Module|ast.ClassDef, name:str):
    results: list[ast.stmt] = []
    for node in namespace.body:
        if isinstance(node, (ast.FunctionDef, ast.ClassDef)):
            if node.name==name:
                results.append(node)
        if isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.target.id==name:
                results.append(node)
    return results

def get_named_node_path(namespace:ast.Module|ast.ClassDef, path:list[str])->list[list[ast.AST]]:
    if len(path)==1:
        return [[namespace, node] for node in get_named_node(namespace, path[0])]
    results: list[list[ast.AST]] = []
    for node in get_named_node(namespace, path[0]):
        if isinstance(node, ast.ClassDef):
            inner_result = get_named_node_path(node, path[1:])
            for inner in inner_result:
                inner.insert(0, namespace)
            results.extend(inner_result)
    return results

class Signature:
    # constants
    FUNCTION:typing.Literal["func"] = "func"
    CLASS:typing.Literal["class"] = "class"
    VARIABLE:typing.Literal["variable"] = "variable"
    # attr
    pathe: list[ast.AST]
    overloads: list[list[ast.AST]]
    level: int
    type: typing.Literal["func", "class", "variable"]

    def __init__(self, module:ast.Module, name_path:str):
        pathes = get_named_node_path(module, name_path.split("."))
        if len(pathes)==0:
            raise RuntimeError(f"Unable to reach '{name_path}'")
        self.level = len(pathes[0])
        self.path = pathes[0]
    
        tp = type(self.path[-1])
        if tp is ast.FunctionDef:
            self.type=self.FUNCTION
        elif tp is ast.ClassDef:
            self.type=self.CLASS
        elif tp is ast.AnnAssign:
            self.type=self.VARIABLE
        else:
            raise RuntimeError(f"Unknown end node type '{tp.__name__}'")
    
        self.overloads = pathes[1:]

        # check overload type
        for ov in self.overloads:
            if type(ov[-1]) is not tp:
                raise RuntimeError(f"Overload type mismatch")
    
    def get_function_signature(self):
        assert self.type==self.FUNCTION
        results = []
        for path in [self.path]+self.overloads:
            node = path[-1]
            if self.type!=self.FUNCTION:
                raise RuntimeError(f"Unable to get signature for '{self.type}'")
            assert isinstance(node, ast.FunctionDef)
            copied = copy.copy(node)
            copied.body = [ast.Expr(ast.Constant(...))]
            results.append(ast.unparse(copied))
        return "\n\n".join(results)
    
    def get_variable_signature(self):
        assert self.type==self.VARIABLE
        return ast.unparse(self.path[-1])

    def get_full_name(self):
        result = []
        for node in self.path:
            if isinstance(node, ast.Module):
                continue
            elif isinstance(node, (ast.FunctionDef, ast.ClassDef)):
                result.append(node.name)
            elif isinstance(node, ast.AnnAssign):
                if not isinstance(node.target, ast.Name):
                    continue
                result.append(node.target.id)
            else:
                raise RuntimeError(f"Unable to get name for '{type(node).__name__}'")
        return ".".join(result)

    def is_property(self):
        if self.type!=self.FUNCTION:
            return False
        node = self.path[-1]
        assert isinstance(node, ast.FunctionDef)
        for deco in node.decorator_list:
            if isinstance(deco, ast.Name) and deco.id=="property":
                return True
        return False
