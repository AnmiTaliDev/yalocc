Name:           yalocc
Version:        @VERSION@
Release:        1%{?dist}
Summary:        Count blank, comment, and code lines in source files
License:        GPL-3.0-only
URL:            @URL@
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  meson
BuildRequires:  gcc

%global debug_package %{nil}

%description
Command-line tool that counts blank, comment, and code lines in source files
and reports totals per language.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install

%files
%license LICENSE
%doc README.md
%{_bindir}/yalocc
%{_datadir}/yalocc/languages.conf
