" vim syntax for shell-fm

if exists("b:current_syntax")
    finish
endif

let b:current_syntax = "shell-fm-rc"

setlocal iskeyword+=-

syn region ShellFMKey start=/^\(\s*[^#]\)\@=/ end=/=\@=/ 
            \ contains=ShellFMKnownKey,ShellFMColorKey,ShellFMKeybindingKey

syn match ShellFMEquals /=/ skipwhite
            \ nextgroup=ShellFMValue

syn keyword ShellFMKnownKey contained
            \ username password default-radio np-file np-file-format np-cmd
            \ pp-cmd bind port extern proxy expiry device title-format minimum
            \ delay-change screen-format term-format download gap discovery
            \ preview-format screen-format term-format unix daemon

syn match ShellFMColorKey /\<[atldsSALTR]-color\>/ contained

syn match ShellFMKeybindingKey /key0x[0-9a-fA-F][0-9a-fA-F]/ contained

syn match ShellFMValue /.*$/ contained 

syn match ShellFMComment "#.*" contains=perlTodo

hi def link ShellFMKnownKey Keyword
hi def link ShellFMColorKey Keyword
hi def link ShellFMKeybindingKey Keyword
hi def link ShellFMValue Constant
hi def link ShellFMComment Comment
