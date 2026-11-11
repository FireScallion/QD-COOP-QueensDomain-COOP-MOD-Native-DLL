using System;using System.Text;using System.Runtime.InteropServices;using System.ComponentModel;using System.Security.Principal;
public static class QuietStation {
 [StructLayout(LayoutKind.Sequential)]struct SA{public int size;public IntPtr descriptor;public int inherit;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]struct SI{public int cb;public string reserved,desktop,title;public int x,y,xs,ys,xc,yc,fill,flags;public short show,reserved2;public IntPtr reservedPtr,stdin,stdout,stderr;}
 [StructLayout(LayoutKind.Sequential)]struct PI{public IntPtr process,thread;public int pid,tid;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern IntPtr CreateWindowStationW(string name,int flags,int access,ref SA sa);
 [DllImport("user32.dll",SetLastError=true)]static extern bool SetProcessWindowStation(IntPtr handle);
 [DllImport("user32.dll")]static extern IntPtr GetProcessWindowStation();
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern IntPtr CreateDesktopW(string name,IntPtr device,IntPtr mode,int flags,int access,ref SA sa);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool GetUserObjectInformationW(IntPtr handle,int index,StringBuilder text,int size,out int needed);
 [DllImport("user32.dll")]static extern bool CloseWindowStation(IntPtr handle);
 [DllImport("user32.dll")]static extern bool CloseDesktop(IntPtr handle);
 [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string text,int rev,out IntPtr descriptor,out int size);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern bool CreateProcessW(string exe,StringBuilder command,IntPtr pa,IntPtr ta,bool inherit,int flags,IntPtr env,string cwd,ref SI startup,out PI info);
 [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll")]static extern int WaitForSingleObject(IntPtr handle,int ms);
 [DllImport("kernel32.dll")]static extern bool GetExitCodeProcess(IntPtr handle,out int code);
 [DllImport("kernel32.dll")]static extern IntPtr LocalFree(IntPtr p);
 public static string StationName(){return Name(GetProcessWindowStation());}
 static string Name(IntPtr p){var s=new StringBuilder(512);int needed;if(!GetUserObjectInformationW(p,2,s,1024,out needed))throw new Win32Exception();return s.ToString();}
 public static int Run(string script,string cwd){
  IntPtr sd=IntPtr.Zero,station=IntPtr.Zero,desktop=IntPtr.Zero;var original=GetProcessWindowStation();int size;PI pi=new PI();
  try{
   string sid=WindowsIdentity.GetCurrent().User.Value;
   if(!ConvertStringSecurityDescriptorToSecurityDescriptorW("D:P(A;;GA;;;"+sid+")",1,out sd,out size))throw new Win32Exception();
   var sa=new SA{size=Marshal.SizeOf(typeof(SA)),descriptor=sd};
   station=CreateWindowStationW(null,0,0x37f,ref sa);if(station==IntPtr.Zero)throw new Win32Exception();
   string name=Name(station);if(name.Equals("WinSta0",StringComparison.OrdinalIgnoreCase))throw new Exception("Refusing interactive station");
   if(!SetProcessWindowStation(station))throw new Win32Exception();
   desktop=CreateDesktopW("QDCoopTests",IntPtr.Zero,IntPtr.Zero,0,0x1ff,ref sa);if(desktop==IntPtr.Zero)throw new Win32Exception();
   if(!SetProcessWindowStation(original))throw new Win32Exception();
   var si=new SI{cb=Marshal.SizeOf(typeof(SI)),desktop=name+"\\QDCoopTests",flags=1,show=0};
   string exe=Environment.GetFolderPath(Environment.SpecialFolder.System)+"\\WindowsPowerShell\\v1.0\\powershell.exe";
   var command=new StringBuilder("\""+exe+"\" -NoProfile -ExecutionPolicy Bypass -File \""+script+"\"");
   if(!CreateProcessW(exe,command,IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,cwd,ref si,out pi))throw new Win32Exception();
   Console.WriteLine("Noninteractive station: "+name+"; child PID "+pi.pid);
   WaitForSingleObject(pi.process,-1);int code;if(!GetExitCodeProcess(pi.process,out code))throw new Win32Exception();return code;
  }finally{SetProcessWindowStation(original);if(pi.thread!=IntPtr.Zero)CloseHandle(pi.thread);if(pi.process!=IntPtr.Zero)CloseHandle(pi.process);if(desktop!=IntPtr.Zero)CloseDesktop(desktop);if(station!=IntPtr.Zero)CloseWindowStation(station);if(sd!=IntPtr.Zero)LocalFree(sd);}
 }
}
