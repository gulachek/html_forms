set path=,,client/**,server/**,test/**

augroup msgstream
	autocmd!
	autocmd BufNewFile *.c,*.h,*.cpp,*.hpp :0r <sfile>:h/vim/template/skeleton.c
augroup END
