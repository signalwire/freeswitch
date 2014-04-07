require 'rbconfig'

cflags = if RUBY_VERSION =~ /1.9/ then
  "-I#{RbConfig::CONFIG['rubyhdrdir']} -I#{RbConfig::CONFIG['rubyhdrdir']}/#{RbConfig::CONFIG['arch']}"
else
  "-I#{RbConfig::CONFIG["topdir"]} -I#{RbConfig::CONFIG['rubyhdrdir']} -I#{RbConfig::CONFIG['rubyhdrdir']}/#{RbConfig::CONFIG['arch']}"
end
puts cflags
