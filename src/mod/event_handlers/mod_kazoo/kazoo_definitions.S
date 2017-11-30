    .global kz_default_config
    .global kz_default_config_size
    .section .rodata
kz_default_config:
    .incbin "kazoo.conf.xml"
1:
kz_default_config_size:
    .int 1b - kz_default_config
