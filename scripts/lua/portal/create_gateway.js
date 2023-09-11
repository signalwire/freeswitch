name = argv[1];
realm = argv[2];
username = argv[3];
password = argv[4];
register = argv[5];

fd = new FileIO(name, "wc");

fd.write(`<gateway name="'${name}'">\n`);
fd.write(`	<param name="realm" value="' ${realm} '"/>\n`);
fd.write(`	<param name="username" value="' ${username} '"/>\n`);
fd.write(`	<param name="password" value="' ${password} '"/>\n`);
fd.write(`	<param name="register" value="' ${register}'"/>\n`);
fd.write(`</gateway>\n`);

apiExecute("sofia profile external rescan");
