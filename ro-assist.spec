Name:           ro-assist
Version:        0.1.0
Release:        1%{?dist}
Summary:        Fedora Linux Sistem Asistanı

License:        GPLv3+
URL:            https://github.com/example/ro-Assist
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel

%description
ro-Assist is a system management and update assistant for Linux systems, designed primarily for Fedora. It provides an intuitive GUI to handle system wide updates including DNF packages, Flatpaks, and Snaps.

%prep
%autosetup -p1

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%{_bindir}/ro-assist

%changelog
* Sat Mar 07 2026 Ebubekir Bulut <mutemet91@gmail.com> - 0.1.0-1
- Initial release with 60% carousel, dynamically managed DNF/Flatpak/Snap updates, language and theme detecting modules.
