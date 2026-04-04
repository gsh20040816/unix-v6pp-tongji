[BITS 32]
[extern main0]

[extern _main]  	;"_main()"定义在support.c中
[extern _atexit]	; "_atexit()"定义在support.c中

global greatstart
greatstart:
	mov eax,1
	mov eax,2
	mov eax,3

;构建选项中的 -nostartfiles 禁止了 g++ 去链接 startup code,
;startup code即是在进入我们用C++编写的main0()函数之前，以及main0()
;退出时执行的代码，其执行的工作是初始化(/销毁)global/static对象。
	call _main		;call our own startup code
	jmp main0
	call _atexit  	;call our own startup code
