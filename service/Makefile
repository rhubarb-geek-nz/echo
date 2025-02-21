# Copyright (c) 2025 Roger Brown.
# Licensed under the MIT License.

OBJDIR=obj\$(VSCMD_ARG_TGT_ARCH)
BINDIR=bin\$(VSCMD_ARG_TGT_ARCH)
APPNAME=echo

all: $(BINDIR)\$(APPNAME).exe

clean:
	if exist $(OBJDIR) rmdir /q /s $(OBJDIR)
	if exist $(BINDIR) rmdir /q /s $(BINDIR)

$(BINDIR)\$(APPNAME).exe: $(OBJDIR)\$(APPNAME).obj $(OBJDIR)\$(APPNAME).res $(BINDIR)
	cl /Fe$@ /MT $(OBJDIR)\$(APPNAME).obj $(OBJDIR)\$(APPNAME).res ws2_32.lib advapi32.lib /link /INCREMENTAL:NO /PDB:NONE
	if not "$(CertificateThumbprint)"=="" signtool sign /a /sha1 $(CertificateThumbprint) /fd SHA256 /t http://timestamp.digicert.com $@

$(OBJDIR)\$(APPNAME).obj: $(APPNAME).c $(OBJDIR)
	cl /c /Fo$@ $(APPNAME).c /DWIN32_LEAN_AND_MEAN

$(OBJDIR)\$(APPNAME).res: $(APPNAME).rc
	rc /r $(RCFLAGS) /fo$@ $(APPNAME).rc

$(BINDIR) $(OBJDIR):
	mkdir $@
