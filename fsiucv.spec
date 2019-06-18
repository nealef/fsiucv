%define name fsiucv
%define libmod %{kvers}-%{krel}%{?dist}.s390x

Name: %{name}
Version: %{kvers}
Release: %{krel}%{?dist}
Summary: IUCV character device driver
Group: networking
URL: http://www.sinenomine.net
Buildroot: /var/tmp/%{name}-buildroot
Source: %{name}-%{fsiucvsrc}.tar.gz
License: GPLv2
Requires: kernel = %{kvers}-%{krel}%{?dist}
BuildRequires: kernel-devel = %{kvers}-%{krel}%{?dist}
ExclusiveArch: s390x

%description
IUCV character device driver. Provides and open/close/read/write interface to IUCV.

%prep
%setup -q -n %{name}-%{fsiucvsrc}

%build
ksrc=%{_usrsrc}/kernels/%{kvers}-%{krel}%{?dist}.%{_target_cpu}
make -C "${ksrc}" modules M=$PWD 
make propjr

%pre

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
install -d ${RPM_BUILD_ROOT}/lib/modules/%{libmod}/extra
install -d ${RPM_BUILD_ROOT}%{_bindir}
install -d ${RPM_BUILD_ROOT}%{_includedir}/net/iucv
install fsiucv.ko ${RPM_BUILD_ROOT}/lib/modules/%{libmod}/extra/fsiucv.ko
install propjr ${RPM_BUILD_ROOT}%{_bindir}/propjr
install fsiucv.h ${RPM_BUILD_ROOT}%{_includedir}/net/iucv/fsiucv.h

%clean
rm -rf %{buildroot}

%post
/sbin/depmod -a
/sbin/modprobe fsiucv

%preun

%postun
rmmod fsiucv
depmod -a

%files
%defattr(-,root,root)
%doc propjr.c 
%license LICENSE.TXT
%attr(0750,root,root) /lib/modules/*/extra/fsiucv.ko
%attr(0750,root,root) %{_bindir}/propjr
%{_includedir}/net/iucv/fsiucv.h
