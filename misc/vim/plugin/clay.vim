nmap <Leader>cl :ClayLibraryModule<SPACE>

if !exists("g:LibClay")
    let g:LibClay = "/usr/local/lib/lib-clay"
endif

function! s:unique(list)
    let dict = {}
    for value in a:list
        let dict[value] = 1
    endfor
    return sort(keys(dict))
endfunction

function! ClayCompleteLibraryModule(arglead, cmdline, cursorpos)
    let modulesl = []
    let modulelead = substitute(a:arglead, "\\.", "/", "g")
    let modules = globpath(g:LibClay, modulelead . "*")
    if modules != ""
        let modulesl = split(modules, "\n")
        let modulesl = filter(modulesl, 'getftype(v:val) == "dir" || matchstr(v:val, "\\.clay$") != ""')
        let modulesl = map(modulesl, 'substitute(v:val, "\\(\\..*\\)\\?\\.clay$", "", "")')
        let modulesl = map(modulesl, 'substitute(v:val, "^\\V" . escape(g:LibClay, "\\") . "\\v[/\\\\]", "", "")')
        let modulesl = s:unique(modulesl)
        let modulesl = map(modulesl, 'substitute(v:val, "/\\|\\\\", ".", "g")')
    endif
    return modulesl
endfunction

command! -nargs=1 -complete=customlist,ClayCompleteLibraryModule ClayLibraryModule :call GoToClayLibraryModule("<args>")

function! ClayModuleFileNames(path)
    let names = [a:path . ".clay"]
    let oses = ["unix", "windows", "linux", "macosx"]
    let cpus = ["x86", "ppc", "arm"]
    let bits = ["32", "64"]
    for bit in bits
        let names += [a:path . "." . bit . ".clay"]
    endfor
    for cpu in cpus
        let names += [a:path . "." . cpu . ".clay"]
        for bit in bits
            let names += [a:path . "." . cpu . "." . bit . ".clay"]
        endfor
    endfor
    for os in oses
        let names += [a:path . "." . os . ".clay"]
        for bit in bits
            let names += [a:path . "." . os . "." . bit . ".clay"]
        endfor
        for cpu in cpus
            let names += [a:path . "." . os . "." . cpu . ".clay"]
            for bit in bits
                let names += [a:path . "." . os . "." . cpu . "." . bit . ".clay"]
            endfor
        endfor
    endfor
    return names
endfunction

function! FindClayModuleFile(path)
    let basename = substitute(a:path, "^.*[/\\\\]", "", "")
    let searchnames = ClayModuleFileNames(a:path)
    let searchnames += ClayModuleFileNames(a:path . "/" . basename)
    echo searchnames
    for name in searchnames
        if getftype(name) != ""
            return name
        endif
    endfor
    return ""
endfunction

function! GoToClayLibraryModule(module)
    let modulefile = FindClayModuleFile(g:LibClay . "/" . substitute(a:module, "\\.", "/", "g"))
    if modulefile == ""
        echo "Library module " modulefile " not found"
    else
        exe "edit " fnameescape(modulefile)
    endif
endfunction
