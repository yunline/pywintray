import macros.pyi_signature as pyi_signature

import black

MODULE_NAME = "pywintray"

module_ast = pyi_signature.load_pyi_ast(MODULE_NAME)

def format_with_black(code):
    return black.format_str(code, mode=black.Mode(line_length=80)) # type:ignore

def get_permalink_name(name):
    return name.replace(".","")

def define_env(env):
    @env.macro
    def API(name:str, brief:str):
        sig = pyi_signature.Signature(module_ast, name)

        heading_level = sig.level # toplevel is h2
        if sig.level==2:
            # toplevel, add module name before
            heading = MODULE_NAME + "." + name
        else:
            heading = name
        
        after_brief = ""
        type_icon = ""
        if sig.type==sig.FUNCTION:
            if sig.is_property():
                type_icon = ":material-code-brackets:"
            else:
                type_icon = ":material-function:"
            after_brief = format_with_black(sig.get_function_signature())
        elif sig.type==sig.CLASS:
            type_icon = ":material-cube-outline:"
        elif sig.type==sig.VARIABLE:
            type_icon = ":material-code-brackets:"
            after_brief = format_with_black(sig.get_variable_signature())
        if after_brief:
            after_brief = f"\n```py {{.yaml .no-copy}}\n{after_brief}\n```\n"

        body = f"\n{'#'*heading_level} <a id={get_permalink_name(name)}></a> {type_icon} `{heading}`\n{brief}{after_brief}"

        return body

    @env.macro
    def REF(name:str, alias=None):
        if alias is None:
            alias = name
        return f"[`{alias}`](../api-reference.md/#{get_permalink_name(name)})"
