Summary: Virtuozzo/OpenVZ disk quota control utility
Name: vzquota
Version: 3.1
Release: 1%{?dist}
License: GPL
Group: System Environment/Kernel
Source: http://download.openvz.org/utils/%{name}/%{version}/src/%{name}-%{version}.tar.bz2
ExclusiveOS: Linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: vzquotamod
URL: http://openvz.org/

%description
This utility allows system administator to control disk quotas
for Virtuozzo/OpenVZ containers.

%prep
%setup

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
* Tue Sep 11 2012 Kir Kolyshkin <kir@openvz.org> - 3.1-1
- New functionality:
- * vzdqdump, vzdqload: add -F to dump/load first level quotas
- Fixes:
- * Make ext4 delalloc work with vzquota (#2035)
- * fix quota init fail then run from deleted directory
- * fix building with fresh toolchain (#1357 etc.)
- * skip quota file locking on 'vzquota show' action
- * fix to work with more than 1 TB quotas
- * correct exit code in case quota file does not exist
- * minor code style fixes
- * whitespace cleanups
- Improvements:
- * quota on: more info on failed quota on with EEXIST
- * quota off: enable detailed info on fail by default
- * quota off: add processing EIO
- * man/*: assorted fixes and formatting improvements
- * man/vzquota: minor improvements (#2310)
- * vzdqdump, vzdqload: fix typo in usage
- * Makefile: Don't strip executable, respect LDFLAGS (#1191)

* Thu Mar 06 2008 Kir Kolyshkin <kir@openvz.org> - 3.0.12-1
- updated description, VE->container terminology change

* Wed Jun 13 2007 Andy Shevchenko <andriy@asplinux.com.ua> - 3.0.9-1
- fixed according to Fedora Packaging Guidelines:
  - use dist tag
  - removed Vendor tag
  - added URL tag
  - use full url for source
  - changed BuildRoot tag

* Mon Oct 09 2006 Dmitry Mishin <dim-at-openvz.org> 3.0.9-1
- added README and NEWS files
- deleted debian directory (requested by debian package maintainers)
- fixed compilation on ppc64 platform.

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
