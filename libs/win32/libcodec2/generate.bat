REM 
REM In order to regenerate headers
REM execute this generate.bat from the "VS2015 x86 Native Tools Command Prompt"
REM 

cd ../../libcodec2-2.59/codec2
cl /EHsc generate_codebook.c
generate_codebook.exe lsp_cb codebook/lsp1.txt codebook/lsp2.txt codebook/lsp3.txt codebook/lsp4.txt codebook/lsp5.txt codebook/lsp6.txt codebook/lsp7.txt codebook/lsp8.txt codebook/lsp9.txt codebook/lsp10.txt > codebook.c
generate_codebook.exe lsp_cbd codebook/dlsp1.txt codebook/dlsp2.txt codebook/dlsp3.txt codebook/dlsp4.txt codebook/dlsp5.txt codebook/dlsp6.txt codebook/dlsp7.txt codebook/dlsp8.txt codebook/dlsp9.txt codebook/dlsp10.txt > codebookd.c
generate_codebook.exe lsp_cbdt codebook/lspdt1.txt codebook/lspdt2.txt codebook/lspdt3.txt codebook/lspdt4.txt codebook/lspdt5.txt codebook/lspdt6.txt codebook/lspdt7.txt codebook/lspdt8.txt codebook/lspdt9.txt codebook/lspdt10.txt > codebookdt.c
generate_codebook.exe lsp_cbvq codebook/lsp1.txt codebook/lsp2.txt codebook/lsp3.txt codebook/lsp4.txt ../unittest/lsp45678910.txt > codebookvq.c
generate_codebook.exe lsp_cbjnd codebook/lsp1.txt codebook/lsp2.txt codebook/lsp3.txt codebook/lsp4.txt ../unittest/lspjnd5-10.txt > codebookjnd.c
generate_codebook.exe lsp_cbjvm codebook/lspjvm1.txt codebook/lspjvm2.txt codebook/lspjvm3.txt > codebookjvm.c
generate_codebook.exe lsp_cbvqanssi codebook/lspvqanssi1.txt codebook/lspvqanssi2.txt codebook/lspvqanssi3.txt codebook/lspvqanssi4.txt > codebookvqanssi.c
generate_codebook.exe ge_cb codebook/gecb.txt > codebookge.c