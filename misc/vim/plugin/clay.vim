nmap <Leader>cl :LibClayModule<SPACE>
nmap <Leader>cn :LibClayNewModule<SPACE>
nmap <Leader>cN :LibClayNewModuleDir<SPACE>
nmap <Leader>c0 :LibClayAlternate 0<CR>
nmap <Leader>c1 :LibClayAlternate 1<CR>
nmap <Leader>c2 :LibClayAlternate 2<CR>
nmap <Leader>c3 :LibClayAlternate 3<CR>
nmap <Leader>c4 :LibClayAlternate 4<CR>
nmap <Leader>c5 :LibClayAlternate 5<CR>
nmap <Leader>c6 :LibClayAlternate 6<CR>
nmap <Leader>c7 :LibClayAlternate 7<CR>
nmap <Leader>c8 :LibClayAlternate 8<CR>
nmap <Leader>c9 :LibClayAlternate 9<CR>

if !exists("g:LibClay")
    let g:LibClay = "/usr/local/lib/lib-clay"
endif

if !exists("g:LibClayAlternates")
    let g:LibClayAlternates = ["/usr/local/lib/lib-clay"]
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

command! -nargs=1 -complete=customlist,ClayCompleteLibraryModule LibClayModule :call GoToLibClayModule("<args>")
command! -nargs=1 -complete=customlist,ClayCompleteLibraryModule LibClayNewModule :call CreateLibClayModule("<args>")
command! -nargs=1 -complete=customlist,ClayCompleteLibraryModule LibClayNewModuleDir :call CreateLibClayModuleDir("<args>")
command! -nargs=1 LibClayAlternate :call SelectLibClayAlternate(<args>)

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
    for name in searchnames
        if getftype(name) != ""
            return name
        endif
    endfor
    return ""
endfunction

function! GoToLibClayModule(module)
    let modulefile = FindClayModuleFile(g:LibClay . "/" . substitute(a:module, "\\.", "/", "g"))
    if modulefile == ""
        echo "Library module" modulefile "not found"
    else
        exe "edit " fnameescape(modulefile)
    endif
endfunction

function! SelectLibClayAlternate(n)
    echo "Setting g:LibClay to" get(g:LibClayAlternates, a:n, g:LibClay)
    let g:LibClay = get(g:LibClayAlternates, a:n, g:LibClay)
endfunction

function! CreateLibClayModule(module)
    let modulename = g:LibClay . "/" . substitute(a:module, "\\.", "/", "g") . ".clay"
    let modulepath = substitute(modulename, "[/\\\\][^/\\\\]*$", "", "")
    exe "silent !mkdir -p " shellescape(modulepath)
    exe "edit " fnameescape(modulename)
endfunction

function! CreateLibClayModuleDir(module)
    let modulepath = g:LibClay . "/" . substitute(a:module, "\\.", "/", "g")
    let basename = substitute(a:module, "^.*\\.", "", "")
    exe "silent !mkdir -p " shellescape(modulepath)
    exe "edit " fnameescape(modulepath . "/" . basename . ".clay")
endfunction
