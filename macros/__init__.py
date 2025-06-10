import macros.pyi_signature as pyi_signature

import black

MODULE_NAME = "pywintray"

module_ast = pyi_signature.load_pyi_ast(MODULE_NAME)

def format_with_black(code):
    return black.format_str(code, mode=black.Mode(line_length=80)) # type:ignore

def get_permalink_name(name):
    return name.replace(".","")

def get_signature_str(sig:pyi_signature.Signature):
    signature_code = ""
    if sig.type==sig.FUNCTION:
        signature_code = format_with_black(sig.get_function_signature())
    elif sig.type==sig.CLASS:
        pass
    elif sig.type==sig.VARIABLE:
        signature_code = format_with_black(sig.get_variable_signature())
    if signature_code:
        signature_code = f"\n```py {{.yaml .no-copy}}\n{signature_code}\n```\n"
    return signature_code

def define_env(env):
    @env.macro
    def SIGNATURE(name:str):
        sig_code = get_signature_str(pyi_signature.Signature(module_ast, name))
        return f"<a id={get_permalink_name(name)}></a>\n{sig_code}"

    @env.macro
    def API(name:str, brief:str, heading:str|None=None):
        """ macro API
        name: the name of the symbol, like "MenuItem.label"
            name is used for searching the symbol, generating the signature
            and generating the permalink

        brief: a brief string describing the API

        heading: by default heading is the name of the symbol
            you can specify your own heading through this parameter too
        """

        sig = pyi_signature.Signature(module_ast, name)

        if heading is None:
            if sig.level==2:
                # toplevel, add module name before
                heading = MODULE_NAME + "." + name
            else:
                heading = name

        signature_code = get_signature_str(sig)

        type_icon = ""
        if sig.type==sig.FUNCTION:
            if sig.is_property():
                type_icon = ":material-code-brackets:"
            else:
                type_icon = ":material-function:"
        elif sig.type==sig.CLASS:
            type_icon = ":material-cube-outline:"
        elif sig.type==sig.VARIABLE:
            type_icon = ":material-code-brackets:"

        body = f"<a id={get_permalink_name(name)}></a> {type_icon} `{heading}`\n{brief}{signature_code}"

        return body

    @env.macro
    def REF(name:str, alias=None):
        if alias is None:
            alias = name
        return f"[`{alias}`](../api-reference.md/#{get_permalink_name(name)})"
