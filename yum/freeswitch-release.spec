Name:           freeswitch-release       
Version:        1 
Release:        0
Summary:        FreeSwitch Packages for Enterprise Linux repository configuration

Group:          System Environment/Base 
License:        GPL 
URL:            http://www.freeswitch.org

Source0:        http://files.freeswitch.org/yum/RPM-GPG-KEY-FREESWITCH
Source1:        GPL	
Source2:        freeswitch.repo	
Source3:        freeswitch-testing.repo	

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:     noarch
Requires:      redhat-release >=  %{version} 

%description
This package contains the FreeSwitch Yum repository
GPG key as well as configuration for yum.

%prep
%setup -q  -c -T
install -pm 644 %{SOURCE0} .
install -pm 644 %{SOURCE1} .

%build


%install
rm -rf $RPM_BUILD_ROOT

#GPG Key
install -Dpm 644 %{SOURCE0} \
    $RPM_BUILD_ROOT%{_sysconfdir}/pki/rpm-gpg/RPM-GPG-KEY-FREESWITCH

# yum
install -dm 755 $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d
install -pm 644 %{SOURCE2} %{SOURCE3}  \
    $RPM_BUILD_ROOT%{_sysconfdir}/yum.repos.d

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc GPL
%config(noreplace) /etc/yum.repos.d/*
/etc/pki/rpm-gpg/*


%changelog
* Sat Jan 27 2012 Ken Rice <krice at freeswitch.org> - 5-0
- Replace GPG key with correct key, and update primary URLs
* Sat Dec 21 2011 Ken Rice <krice at freeswitch.org> - 5-0
- Initial Version - Thanks to the EPEL Guys I had something to Rip Off. Hense this package is GPL
