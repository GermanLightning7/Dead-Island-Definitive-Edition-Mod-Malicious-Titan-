using System;using System.IO;using System.Linq;using System.Diagnostics;using System.Runtime.InteropServices;using System.Security.Cryptography;using System.Text;using System.Text.RegularExpressions;using System.Threading;
public static class Core {
 public const string MainDll="JentaSpecialEdition.dll";
 public const string GameHash="d10c7f59ad3bf62e7f2dd69cd7e49eb634d4e8d046e4f4c07463b5d589833545";
 public const string EngineHash="7cf1f0153a2748da55a36d89fcf2e451ba444468260355836722d3ef76b27c79";
 public static string Root(string p){return Path.GetFullPath(p).TrimEnd(Path.DirectorySeparatorChar);}
 public static string Home(string game){return Path.Combine(game,"Jenta_Special_Edition");}
 public static string FileHash(string p){using(var f=File.OpenRead(p))using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(f)).Replace("-","").ToLowerInvariant();}
 public static void NoLinks(string path){var p=Path.GetFullPath(path);while(!string.IsNullOrEmpty(p)){if((File.Exists(p)||Directory.Exists(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new Exception("A linked folder/file is not supported: "+p);p=Path.GetDirectoryName(p);}}
 public static bool IsSteamInstall(string game){
  try{
   game=Root(game);var common=Directory.GetParent(game);var apps=common==null?null:common.Parent;
   if(common==null||apps==null||!common.Name.Equals("common",StringComparison.OrdinalIgnoreCase)||!apps.Name.Equals("steamapps",StringComparison.OrdinalIgnoreCase))return false;
   var manifest=Path.Combine(apps.FullName,"appmanifest_383150.acf");if(!File.Exists(manifest))return false;NoLinks(manifest);
   var text=File.ReadAllText(manifest);var app=Regex.Match(text,"\"appid\"\\s*\"([0-9]+)\"");var dir=Regex.Match(text,"\"installdir\"\\s*\"([^\"]+)\"");
   if(!app.Success||app.Groups[1].Value!="383150"||!dir.Success)return false;
   var name=dir.Groups[1].Value;if(name=="."||name==".."||name.IndexOfAny(new[]{'/', '\\', ':'})>=0)return false;
   return Root(Path.Combine(common.FullName,name)).Equals(game,StringComparison.OrdinalIgnoreCase);
  }catch{return false;}
 }
 public static void Validate(string game){
  game=Root(game);NoLinks(game);
  if(!IsSteamInstall(game))throw new Exception("Select the Steam installation of Dead Island Definitive Edition (app 383150). The folder must match Steam appmanifest_383150.acf. In Steam: Properties > Installed Files > Browse.");
  if(!File.Exists(Path.Combine(game,"DeadIslandGame.exe"))||!Directory.Exists(Path.Combine(game,"DI")))throw new Exception("Select the Dead Island Definitive Edition folder containing DeadIslandGame.exe and DI.");
  if(FileHash(Path.Combine(game,"gamedll_x64_rwdi.dll"))!=GameHash||FileHash(Path.Combine(game,"engine_x64_rwdi.dll"))!=EngineHash)throw new Exception("This game build does not match the supported Definitive Edition build. Nothing was installed.");
 }
 public static void ValidateFiles(string game){
  if(!File.Exists(Path.Combine(game,@"Jenta_Special_Edition/Native/DideMeleeDurability.dll"))||FileHash(Path.Combine(game,@"Jenta_Special_Edition/Native/DideMeleeDurability.dll"))!="27a57962ae3b1ea5e9b389fef63fbe10245b5bdfeee4f8b69b95237c800f78bd")throw new Exception("Manual package file missing or changed: Jenta_Special_Edition/Native/DideMeleeDurability.dll");
  if(!File.Exists(Path.Combine(game,@"Jenta_Special_Edition/Native/DideMeleeNative.dll"))||FileHash(Path.Combine(game,@"Jenta_Special_Edition/Native/DideMeleeNative.dll"))!="95eb8b6f0f8799bb56e2141e4d4c0b443898226eef6530f5a9bca1107d3d0f93")throw new Exception("Manual package file missing or changed: Jenta_Special_Edition/Native/DideMeleeNative.dll");
  if(!File.Exists(Path.Combine(game,@"Jenta_Special_Edition/Native/JentaSpecialEdition.dll"))||FileHash(Path.Combine(game,@"Jenta_Special_Edition/Native/JentaSpecialEdition.dll"))!="76df8d7bf1190fd7d08c640bf77d4d1ade2cc7c780d6d8c98d085694fc3cf29e")throw new Exception("Manual package file missing or changed: Jenta_Special_Edition/Native/JentaSpecialEdition.dll");
  if(!File.Exists(Path.Combine(game,@"concrt140.dll"))||FileHash(Path.Combine(game,@"concrt140.dll"))!="2405355f0a58067b258f8df33c327e3a3d716eaac5a3a5aebb757842d85bd376")throw new Exception("Manual package file missing or changed: concrt140.dll");
  if(!File.Exists(Path.Combine(game,@"msvcp140.dll"))||FileHash(Path.Combine(game,@"msvcp140.dll"))!="0f885b509a685d2bbfa652fed26b5fb31d88fbdab0a978c641d1c7b8aa460aa9")throw new Exception("Manual package file missing or changed: msvcp140.dll");
  if(!File.Exists(Path.Combine(game,@"msvcp140_1.dll"))||FileHash(Path.Combine(game,@"msvcp140_1.dll"))!="bfad5aef4c63a669e3c140655cdfdf395b6c979b400a447bd5dcb65ed8826c3d")throw new Exception("Manual package file missing or changed: msvcp140_1.dll");
  if(!File.Exists(Path.Combine(game,@"msvcp140_2.dll"))||FileHash(Path.Combine(game,@"msvcp140_2.dll"))!="3ea06f0ee098b4823cb79599df3780e7f23cce52c19aac31d2a0d47efe33a5e9")throw new Exception("Manual package file missing or changed: msvcp140_2.dll");
  if(!File.Exists(Path.Combine(game,@"msvcp140_atomic_wait.dll"))||FileHash(Path.Combine(game,@"msvcp140_atomic_wait.dll"))!="640b2aefced484d0368eea5bdd06addd0658a3a70a49256e560d6923b404a479")throw new Exception("Manual package file missing or changed: msvcp140_atomic_wait.dll");
  if(!File.Exists(Path.Combine(game,@"msvcp140_codecvt_ids.dll"))||FileHash(Path.Combine(game,@"msvcp140_codecvt_ids.dll"))!="f2069a52880ec885ee7f0511186100eb7fada0411a2b4948fafea7735b878a18")throw new Exception("Manual package file missing or changed: msvcp140_codecvt_ids.dll");
  if(!File.Exists(Path.Combine(game,@"vccorlib140.dll"))||FileHash(Path.Combine(game,@"vccorlib140.dll"))!="19839407c3fdbc824e5bce189bf68ddf8097f12ec28b757797ffa0415c144ddd")throw new Exception("Manual package file missing or changed: vccorlib140.dll");
  if(!File.Exists(Path.Combine(game,@"vcruntime140.dll"))||FileHash(Path.Combine(game,@"vcruntime140.dll"))!="d5e4d9a3e835fa679450145d6a7d94e36573a509317111904d9b3712c30d9066")throw new Exception("Manual package file missing or changed: vcruntime140.dll");
  if(!File.Exists(Path.Combine(game,@"vcruntime140_1.dll"))||FileHash(Path.Combine(game,@"vcruntime140_1.dll"))!="1f2d41c4aa5db0bc33ebf7b66d72943a817d7ce6cbe880502a9403823633093f")throw new Exception("Manual package file missing or changed: vcruntime140_1.dll");
  if(!File.Exists(Path.Combine(game,@"vcruntime140_threads.dll"))||FileHash(Path.Combine(game,@"vcruntime140_threads.dll"))!="219915cf20822f34d5e7c1fdd4e21ae7f3396881096c51036225fb8f84b47afa")throw new Exception("Manual package file missing or changed: vcruntime140_threads.dll");
 }
}
public static class Launcher {
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,int id);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr at,UIntPtr size,uint type,uint protect);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr p,IntPtr at,byte[] data,UIntPtr size,out UIntPtr written);
 [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr p,IntPtr attr,UIntPtr stack,IntPtr fn,IntPtr arg,uint flags,IntPtr tid);
 [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr p,IntPtr at,UIntPtr size,uint type);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
 [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
 static ProcessModule Module(Process p,string name){p.Refresh();return p.Modules.Cast<ProcessModule>().FirstOrDefault(m=>m.ModuleName.Equals(name,StringComparison.OrdinalIgnoreCase));}
 static void Load(Process p,string path){
  var existing=Module(p,Path.GetFileName(path));if(existing!=null){if(Core.FileHash(existing.FileName)!=Core.FileHash(path))throw new Exception("A different menu version is already loaded. Exit the game and disable the old loader before retrying.");return;}
  IntPtr local=GetProcAddress(GetModuleHandle("kernel32.dll"),"LoadLibraryW");var owner=Process.GetCurrentProcess().Modules.Cast<ProcessModule>().First(m=>local.ToInt64()>=m.BaseAddress.ToInt64()&&local.ToInt64()<m.BaseAddress.ToInt64()+m.ModuleMemorySize);var remote=Module(p,owner.ModuleName);if(remote==null)throw new Exception("Windows loader module is not ready.");var entry=new IntPtr(remote.BaseAddress.ToInt64()+local.ToInt64()-owner.BaseAddress.ToInt64());
  IntPtr handle=OpenProcess(0x43A,false,p.Id),mem=IntPtr.Zero,thread=IntPtr.Zero;bool done=false;if(handle==IntPtr.Zero)throw new Exception("Cannot access the game. Run this launcher as administrator if the game is elevated.");
  try{byte[] data=Encoding.Unicode.GetBytes(path+"\0");mem=VirtualAllocEx(handle,IntPtr.Zero,(UIntPtr)data.Length,0x3000,4);UIntPtr written;if(mem==IntPtr.Zero||!WriteProcessMemory(handle,mem,data,(UIntPtr)data.Length,out written)||written.ToUInt64()!=(ulong)data.Length)throw new Exception("Could not prepare menu loading.");thread=CreateRemoteThread(handle,IntPtr.Zero,UIntPtr.Zero,entry,mem,0,IntPtr.Zero);if(thread==IntPtr.Zero)throw new Exception("Could not load the menu.");done=WaitForSingleObject(thread,15000)==0;if(!done)throw new Exception("Menu load timed out. Exit the game before retrying.");if(Module(p,Path.GetFileName(path))==null)throw new Exception("Windows did not load the menu. Check the installed runtime files.");}
  finally{if(mem!=IntPtr.Zero&&(done||thread==IntPtr.Zero))VirtualFreeEx(handle,mem,UIntPtr.Zero,0x8000);if(thread!=IntPtr.Zero)CloseHandle(thread);CloseHandle(handle);}
 }
 public static void Run(string game,Action<string> report){
  game=Core.Root(game);Core.Validate(game);Core.ValidateFiles(game);
  report("Starting Dead Island through Steam...");Process.Start(new ProcessStartInfo("steam://rungameid/383150"){UseShellExecute=true});var deadline=DateTime.UtcNow.AddSeconds(180);Process process=null;
  while(DateTime.UtcNow<deadline){foreach(var p in Process.GetProcessesByName("DeadIslandGame")){try{if(!Core.Root(Path.GetDirectoryName(p.MainModule.FileName)).Equals(game,StringComparison.OrdinalIgnoreCase))continue;if(Module(p,"gamedll_x64_rwdi.dll")!=null&&Module(p,"engine_x64_rwdi.dll")!=null){process=p;break;}}catch(System.ComponentModel.Win32Exception){}}if(process!=null)break;Thread.Sleep(500);}
  if(process==null)throw new Exception("The selected game did not start within three minutes. Sign in to Steam, then try Launch again.");
  if(Module(process,"SilentAimPersistentCollisionRedirectV2.dll")!=null)throw new Exception("Another menu is already loaded. Close the game and disable its loader first.");
  report("Loading Jenta's Special Edition...");Thread.Sleep(1500);Load(process,Path.Combine(Core.Home(game),"Native",Core.MainDll));
  deadline=DateTime.UtcNow.AddSeconds(20);while(DateTime.UtcNow<deadline){if(process.HasExited)throw new Exception("The game closed during startup.");if(Module(process,"DideMeleeNative.dll")!=null&&Module(process,"DideMeleeDurability.dll")!=null){report("MENU LOADED. Return to the game and press Insert. All three native modules are loaded.");return;}Thread.Sleep(500);}throw new Exception("Main menu loaded, but a companion module did not load. Close the game and verify the installation.");
 }
}

public static class Program {
 [STAThread] public static int Main(string[] args){
  if(args.Length==2&&args[0]=="--check"){try{Core.Validate(args[1]);Core.ValidateFiles(args[1]);return 0;}catch{return 1;}}
  System.Windows.Forms.Application.EnableVisualStyles();
  var form=new System.Windows.Forms.Form{Text="Jenta's Special Edition",Width=560,Height=180,StartPosition=System.Windows.Forms.FormStartPosition.CenterScreen,BackColor=System.Drawing.Color.FromArgb(22,16,36),ForeColor=System.Drawing.Color.FromArgb(243,236,255),FormBorderStyle=System.Windows.Forms.FormBorderStyle.FixedDialog,MaximizeBox=false};
  var label=new System.Windows.Forms.Label{Text="Verifying Jenta's Special Edition...",Dock=System.Windows.Forms.DockStyle.Fill,Padding=new System.Windows.Forms.Padding(24),Font=new System.Drawing.Font("Segoe UI",12)};
  form.Controls.Add(label);
  form.Shown+=(sender,eventArgs)=>{
   var worker=new Thread(()=>{
    try{
     string game=Directory.GetParent(AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar)).FullName;
     Launcher.Run(game,message=>{if(!form.IsDisposed)form.BeginInvoke((Action)(()=>label.Text=message));});
     if(!form.IsDisposed)form.BeginInvoke((Action)(()=>label.Text="Jenta's Special Edition loaded. Press Insert in game. You can close this window."));
    }catch(Exception error){if(!form.IsDisposed)form.BeginInvoke((Action)(()=>{label.Text="Launch stopped. Check the package and game version.";System.Windows.Forms.MessageBox.Show(form,error.Message,"Jenta's Special Edition",System.Windows.Forms.MessageBoxButtons.OK,System.Windows.Forms.MessageBoxIcon.Error);}));}
   });worker.IsBackground=true;worker.Start();
  };
  System.Windows.Forms.Application.Run(form);return 0;
 }
}
