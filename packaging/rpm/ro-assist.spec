%global debug_package %{nil}

Name:           ro-assist
Version:        0.2.4
Release:        1%{?dist}
Summary:        First-run and maintenance center for Ro-ASD systems
ExclusiveArch:  x86_64 aarch64

License:        GPLv3+
URL:            https://github.com/Project-Ro-ASD/ro-Assist
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qttools-devel
BuildRequires:  desktop-file-utils
BuildRequires:  appstream

%description
ro-Assist is a first-run and maintenance center for Ro-ASD systems. It guides users through controlled DNF, Flatpak, and Snap update workflows, basic setup actions, support links, and safe handoff to ro Control for hardware and driver status.

%prep
%autosetup -p1

%build
%cmake
%cmake_build

%check
ctest --test-dir redhat-linux-build --output-on-failure

%install
%cmake_install

%files
%{_bindir}/ro-assist
%{_libexecdir}/ro-assist/ro-assist
%{_datadir}/applications/ro-assist.desktop
%config(noreplace) /etc/xdg/autostart/ro-assist-autostart.desktop
%{_datadir}/metainfo/io.github.project_ro_asd.ro_assist.metainfo.xml
%{_datadir}/icons/hicolor/scalable/apps/ro-assist.svg

%changelog
* Sun Sep 20 2026 Project Ro-ASD <contact@roasd.org> - 0.2.4-1
- Capture the draft GitHub Release ID directly from the creation response to avoid list/read-after-write races.
- Retry the Ro-Repo V2 production-signing canary without application behavior changes.

* Sun Sep 20 2026 Project Ro-ASD <contact@roasd.org> - 0.2.3-1
- Fix the release canary environment by installing createrepo_c before local-repository validation.
- Retry the Ro-Repo V2 production-signing canary without application behavior changes.

* Sun Sep 20 2026 Project Ro-ASD <contact@roasd.org> - 0.2.2-1
- Prepare a Ro-Repo V2 production-signing canary release with no application behavior changes.
- Exercise the exact Fedora 44 RPM/SRPM, manifest, checksum, attestation, acceptance, and signing chain.

* Mon Sep 07 2026 Project Ro-ASD <contact@roasd.org> - 0.2.1-1
- Improve responsive layouts and the dark-theme About panel.
- Run system-risk discovery asynchronously to keep the interface responsive.
- Bundle the Ro-ASD dashboard logo and harden package/CI validation.

* Fri Aug 28 2026 Project Ro-ASD <contact@roasd.org> - 0.2.0-2
- Rebuild for Fedora 44 and publish canonical binary and source RPM artifacts.

* Fri Jul 10 2026 Project Ro-ASD <contact@roasd.org> - 0.2.0-1
- Harden maintenance workflow with split DNF, Flatpak, and Snap update steps.
- Add NVIDIA nouveau risk detection, reboot warnings, and ro Control handoff.
- Reposition ro-Assist as the Ro-ASD first-run and maintenance center.

* Wed Jun 24 2026 Project Ro-ASD <contact@roasd.org> - 0.1.2-1
- Add the first-run welcome flow and a reusable dashboard.
- Add printer and scanner support controls through the ro-printer-support package.
- Improve system locale detection and add German and French UI coverage.

* Sat May 09 2026 Ebubekir Bulut <ebubekir.bulut99@gmail.com> - 0.1.1-1
- Refresh package metadata shown in Discover, including author, license, and descriptions.

* Sat Mar 07 2026 Ebubekir Bulut <mutemet91@gmail.com> - 0.1.0-1
- Initial release with 60% carousel, dynamically managed DNF/Flatpak/Snap updates, language and theme detecting modules.
