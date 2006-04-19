Summary: Virtuozzo disk quota control utility
Name: vzquota
Version: 3.0.0
Release: 6
Vendor: SWsoft
License: GPL
Group: System Environment/Kernel
Source: vzquota-%{version}-%{release}.tar.bz2 
ExclusiveOS: Linux
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Requires: vzquotamod

%description
Virtual Private Servers as a part of Virtuozzo product family
are full isolated "virtual machines" available for a user without total
hardware emulation like solutions of VMware type.
This utility allows system administator to control disk quotas
for such environments.

%prep
%setup -n %{name}-%{version}-%{release}

%build
CFLAGS="$RPM_OPT_FLAGS" make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT MANDIR=%{_mandir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(755,root,root) %{_sbindir}/vzquota
%attr(755,root,root) %{_sbindir}/vzdqcheck
%attr(755,root,root) %{_sbindir}/vzdqdump
%attr(755,root,root) %{_sbindir}/vzdqload
%attr(755,root,root) %{_var}/vzquota
%attr(644,root,root) %{_mandir}/man8/vzquota.8*
%attr(644,root,root) %{_mandir}/man8/vzdqcheck.8*
%attr(644,root,root) %{_mandir}/man8/vzdqdump.8*
%attr(644,root,root) %{_mandir}/man8/vzdqload.8*

%changelog
* Tue Apr 18 2006 Kir Kolyshkin <kir-at-openvz.org> 3.0.0-5
- fixed license in man pages

* Wed Mar 15 2006 Andrey Mirkin <amirkin-at-sw.ru> 3.0.0-3
- added new function to reload 2nd level quota

* Mon Feb  6 2006 Kir Kolyshkin <kir-at-openvz.org> 3.0.0-2
- fixed gcc4 compilation issue

* Fri Sep 09 2005 Dmitry Mishin <dim_at_sw.ru> 2.7.0-7
- fixes to use new vzkernel headers provisioning scheme

* Thu Aug 11 2005 Dmitry Mishin <dim_at_sw.ru> 2.7.0-5
- reworked hard links check
- mans fixes

* Sat Aug 06 2005 Dmitry Mishin <dim_at_sw.ru> 2.7.0-4
- adopted for new vzctl_quota ioctls
