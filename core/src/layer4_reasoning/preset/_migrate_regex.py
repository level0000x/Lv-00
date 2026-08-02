import re
import sys
import os

def migrate_file(filepath, category):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Determine the function prefix and macro name based on the file
    # We'll detect them from the content
    original_len = len(content)
    
    # Step 1: Remove wrapper function and REGISTER_XXX macro
    # Detect the wrapper function pattern
    wrapper_match = re.search(r'static bool register_\w+_preset\([^)]+\)\s*\{[^}]+preset_blocks_register_simple[^;]+;\s*\}', content)
    if wrapper_match:
        wrapper_text = wrapper_match.group(0)
        print(f"Found wrapper: {wrapper_text[:60]}...")
    
    # Detect the REGISTER_XXX macro
    macro_match = re.search(r'#define REGISTER_\w+\([^)]+\)\s*\\\s*do\s*\\[^}]+}\s*while\s*\(0\)', content)
    if macro_match:
        macro_text = macro_match.group(0)
        print(f"Found macro: {macro_text[:60]}...")
    
    # Step 2: Replace REGISTER_ADV calls
    # Pattern: { ... PresetType inputs[] = {TYPES}; REGISTER_ADV(ARGS); }
    # We need to handle multi-line strings
    
    # Find all REGISTER_ADV calls and their surrounding blocks
    blocks = []
    pattern = r'(\s*\{\s*/\*[^*]*\*/\s*)?PresetType\s+inputs\[\]\s*=\s*\{(.*?)\}\s*;\s*REGISTER_(\w+)\((.*?)\)\s*;\s*\}'
    
    # Use a more precise approach - find each REGISTER_XXX call and its preceding inputs array
    register_pattern = r'REGISTER_(\w+)\('
    
    idx = 0
    replacements = []
    
    while True:
        m = re.search(register_pattern, content[idx:])
        if not m:
            break
        
        macro_name = m.group(1)
        call_start = idx + m.start()
        call_end = idx + m.end() - 1  # Position after the opening paren
        
        # Find the matching closing paren, handling nested parens
        depth = 1
        pos = call_end
        while depth > 0 and pos < len(content):
            pos += 1
            if content[pos] == '(':
                depth += 1
            elif content[pos] == ')':
                depth -= 1
        call_end = pos + 1  # Include the closing paren
        
        # Get the full REGISTER_XXX(...) call text
        call_text = content[call_start:call_end]
        
        # Find the preceding "{ PresetType inputs[] = {...};" block
        # Look backwards from call_start for the inputs array
        before_call = content[:call_start]
        inputs_match = re.search(r'PresetType\s+inputs\[\]\s*=\s*\{(.*?)\}\s*;', before_call)
        if inputs_match:
            inputs_types = inputs_match.group(1).strip()
            # Find the start of the block (the { before PresetType)
            block_start = before_call.rfind('{', 0, inputs_match.start())
            # Find the end of the block (the } after the REGISTER_XXX call)
            block_end = call_end
            # Find the closing } of the block
            while block_end < len(content) and content[block_end] != '}':
                block_end += 1
            block_end += 1  # Include the closing }
            
            block_text = content[block_start:block_end]
            
            # Extract the arguments from REGISTER_XXX(...)
            args_text = call_text[call_text.index('(')+1:call_text.rindex(')')]
            
            # Parse the arguments (name, desc, inputs, in_count, output, math, comp, cons, rev)
            # This is tricky because of multi-line strings
            # Let's use a simple approach: split by top-level commas
            
            args = []
            depth = 0
            current = ''
            in_string = False
            string_char = None
            
            for ch in args_text:
                if in_string:
                    current += ch
                    if ch == '\\':
                        # Skip the next character
                        pass
                    elif ch == string_char:
                        in_string = False
                elif ch in ('"',):
                    in_string = True
                    string_char = ch
                    current += ch
                elif ch == '(':
                    depth += 1
                    current += ch
                elif ch == ')':
                    depth -= 1
                    current += ch
                elif ch == ',' and depth == 0:
                    args.append(current.strip())
                    current = ''
                else:
                    current += ch
            if current.strip():
                args.append(current.strip())
            
            # args should be: name, desc, inputs_var, in_count, output, math, comp, cons, rev
            if len(args) >= 9:
                name = args[0]
                desc = args[1]
                # inputs_var = args[2] (skip - it's the variable name "inputs")
                in_count = args[3]
                output = args[4]
                math = args[5]
                comp = args[6]
                cons = args[7]
                rev = args[8]
                rest = args[9:] if len(args) > 9 else []
                
                # Check if there are any comments between { and PresetType
                comment_match = re.search(r'/\*[^*]*\*/', before_call[before_call.rfind('{', 0, inputs_match.start()):inputs_match.start()])
                comment = comment_match.group(0) + '\n        ' if comment_match else ''
                
                # Build the new LV_PRESET_REGISTER call
                new_call = f'LV_PRESET_REGISTER(success_count, {name}, {desc}, {in_count}, {output}, {math}, {comp}, {cons}, {rev}, {inputs_types})'
                new_block = '    ' + new_call + ';'
                
                old_block = content[block_start:block_end]
                replacements.append((old_block, new_block))
                
                idx = block_end
            else:
                print(f"WARNING: Could not parse args for {call_text[:50]}... (got {len(args)} args)")
                idx = call_end
        else:
            print(f"WARNING: No inputs array found before {call_text[:50]}...")
            idx = call_end
    
    # Apply replacements in reverse order to preserve positions
    print(f"Found {len(replacements)} blocks to replace")
    for old, new in reversed(replacements):
        content = content.replace(old, new)
    
    # Remove the wrapper function and macro
    # Find the wrapper function
    pattern = r'static bool register_\w+_preset\(const char \*name, const char \*description, const PresetType \*input_types,\s+int input_count, PresetType output_type, const char \*math_def,\s+const char \*complexity, bool is_constructive, bool is_reversible\) \{\s+return preset_blocks_register_simple\(name, description, \w+, input_types, input_count,\s+output_type, math_def, complexity, is_constructive, is_reversible\);\s+\}'
    
    wrapper_match = re.search(pattern, content)
    if wrapper_match:
        wrapper_start = wrapper_match.start()
        # Find the end of the REGISTER_XXX macro (after the wrapper)
        macro_match = re.search(r'#define REGISTER_\w+\([^)]+\)\s*\\\s*do\s*\\[^}]+}\s*while\s*\(0\)', content[wrapper_start:])
        if macro_match:
            macro_end = wrapper_start + macro_match.end()
            # Find the module register function
            register_func = content.find('bool preset_', macro_end)
            if register_func < 0:
                register_func = content.find('bool ', macro_end)
            
            # Check what's between macro_end and register_func
            between = content[macro_end:register_func]
            # Find the first blank line or comment separator
            # The replacement should be: LV_DECLARE_PRESET_REGISTER(CATEGORY) + the separator
            sep_match = re.search(r'\n\s*\n\s*/\*', content[macro_end:])
            if sep_match:
                sep_end = macro_end + sep_match.start() + 1  # include one newline
            else:
                sep_end = macro_end
            
            # Keep the comment separator
            rest = content[macro_end:]
            sep_end = macro_end
            while sep_end < len(rest) and rest[sep_end - macro_end] in '\n\r ':
                sep_end += 1
            # Go back to include the separator comment
            # Find the next "/* ======" or similar
            sep_idx = content.find('/* ============', macro_end)
            if sep_idx >= 0:
                # Find the preceding newlines
                before_sep = sep_idx
                while before_sep > macro_end and content[before_sep-1] in '\n\r ':
                    before_sep -= 1
                
                new_content = content[:wrapper_start] + f'LV_DECLARE_PRESET_REGISTER({category})\n\n' + content[before_sep:]
                content = new_content
                print(f"Removed wrapper+macro, replaced with LV_DECLARE_PRESET_REGISTER({category})")
            else:
                # Fallback
                new_content = content[:wrapper_start] + f'LV_DECLARE_PRESET_REGISTER({category})' + content[macro_end:]
                content = new_content
                print(f"Removed wrapper+macro (fallback)")
        else:
            print(f"WARNING: Could not find REGISTER_XXX macro")
    else:
        print(f"WARNING: Could not find wrapper function")
    
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"File written. Size: {len(content)} bytes (was {original_len})")
    return True

if __name__ == '__main__':
    filepath = sys.argv[1]
    category = sys.argv[2]
    migrate_file(filepath, category)