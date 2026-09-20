namespace MenuPresets {
struct Setting {std::string name;double low,high,normal;bool integer;std::function<double()> get;std::function<bool(double)> set;};
std::vector<Setting> settings;
std::filesystem::path overrideDirectory;
void Add(const char*name,double lo,double hi,double normal,bool integer,std::function<double()> get,std::function<bool(double)> set){settings.push_back({name,lo,hi,normal,integer,get,set});}
template<class T>void Atom(const char*name,std::atomic<T>&value,double lo,double hi,double normal,bool integer=true){auto*p=&value;Add(name,lo,hi,normal,integer,[p]{return(double)p->load();},[p](double x){*p=(T)x;return true;});}
void Bool(const char*name,std::atomic_bool&value,std::atomic_bool*available=nullptr){auto*p=&value;Add(name,0,1,0,true,[p]{return p->load()?1.:0.;},[p,available](double x){if(x&&available&&!available->load())return false;*p=x!=0;return true;});}
void Init(){if(!settings.empty())return;
 Atom("spin_speed",Spinbot::speed,1,10,3);
 Bool("aim",enabled);Bool("fov_limit",fovAimEnabled);Atom("fov_radius",fovRadiusPixels,50,800,250);Add("crosshair_priority",0,1,0,true,[]{return AimTargeting::priority.load()==2?1.:0.;},[](double x){crosshairPriorityEnabled=x!=0;AimTargeting::priority=x?2:0;return true;});Bool("target_marker",targetMarkerEnabled);Bool("target_snapline",targetSnaplineEnabled);
 Add("target_priority",0,2,0,true,[]{return (double)AimTargeting::priority.load();},[](double x){AimTargeting::priority=(int)x;crosshairPriorityEnabled=x==2;return true;});Atom("aim_body_part",AimTargeting::bodyPart,0,1,0);
 Bool("esp",espEnabled);Bool("esp_boxes",espBoxesEnabled);Bool("esp_skeletons",espSkeletonsEnabled);Bool("esp_snaplines",espSnaplinesEnabled);Bool("esp_health",espHealthEnabled);Bool("esp_distance",espDistanceEnabled);
 Bool("stamina",staminaOn,&staminaReady);Atom("jump_scale",jumpScale,.25,20,1,false);Atom("run_scale",runScale,.25,10,1,false);Atom("swing_scale",swingScale,.25,10,1,false);
 Add("god_mode",0,1,0,true,[]{return godModeEnabled?1.:0.;},[](double x){return !x&&!godModeEnabled?true:SetGodModeNative(x!=0);});
 Add("air_jumps",0,1,0,true,[]{return doubleJumpOn?1.:0.;},[](double x){if(x&&!doubleJumpReady)return false;SetDoubleJump(x!=0);return doubleJumpOn.load()==(x!=0);});
 Add("chapters",0,1,0,true,[]{return ChapterUnlock::enabled?1.:0.;},[](double x){return (bool)x==ChapterUnlock::enabled.load()||ChapterUnlock::Set(x!=0);});
 Bool("flashlight",VisualToggles::infiniteFlashlight,&VisualToggles::ready);
 Bool("third_person",CameraFeatures::third,&CameraFeatures::ready);Bool("custom_fov",CameraFeatures::customFov,&CameraFeatures::ready);
 Atom("first_fov",CameraFeatures::firstFov,50,120,75);Atom("third_fov",CameraFeatures::thirdFov,50,120,75);Atom("camera_distance",CameraFeatures::distanceCm,100,600,300);
 Add("ignore_bounds",0,1,0,true,[]{return CameraFeatures::bounds?1.:0.;},[](double x){return (bool)x==CameraFeatures::bounds.load()||CameraFeatures::SetBounds(x!=0);});
 for(int i=0;i<7;i++){auto name="graphics_"+std::to_string(i);Add(name.c_str(),0,1,0,true,[i]{return(GraphicsToggles::disabled.load()&(1u<<i))?1.:0.;},[i](double x){if(x&&!GraphicsToggles::Ready(i))return false;if(x)GraphicsToggles::disabled.fetch_or(1u<<i);else GraphicsToggles::disabled.fetch_and(~(1u<<i));return true;});}
 for(int i=0;i<10;i++)if(auto p=FunToggle(i)){auto name="fun_"+std::to_string(i);Add(name.c_str(),0,1,0,true,[p]{return p->load()?1.:0.;},[p,i](double x){if(x&&!FunReady(i))return false;*p=x!=0;return true;});}
 Atom("grapple_speed",grappleSpeed,10,120,45,false);
 auto companion=[](const char*name,MeleeStateFn*state,MeleeSetFn*set){Add(name,0,1,0,true,[state]{return *state&&(*state)()==1?1.:0.;},[state,set](double x){if(x&&*set==oneShotMenuSet&&!damageReady.load())return false;if(!*set||!*state)return x==0;if((*state)()<0)return x==0;return (*set)((int)x)>=0;});};
 companion("melee",&meleeMenuState,&meleeMenuSet);companion("durability",&wearMenuState,&wearMenuSet);companion("one_shot",&oneShotMenuState,&oneShotMenuSet);companion("chain_melee",&chainMenuState,&chainMenuSet);
 Add("explosive_melee",0,1,0,true,[]{return explosiveMeleeOn?1.:0.;},[](double x){if(x&&!explosiveMeleeReady)return false;explosiveMeleeOn=x!=0;return true;});
 Add("critical",0,1,0,true,[]{return alwaysCriticalEnabled?1.:0.;},[](double x){SetAlwaysCritical(x!=0);return true;});
 Add("explosive_aim",0,1,0,true,[]{return explosiveSilentAimEnabled?1.:0.;},[](double x){SetExplosiveSilentAim(x!=0);return true;});
 Add("shot_type",0,3,0,true,[]{return(double)weaponShotTypeMode.load();},[](double x){SetShotTypeMode((int)x);return true;});
 Add("weapon_profile",0,3,0,true,[]{return(double)weaponProfileType.load();},[](double x){SetWeaponProfileType((int)x);return true;});
 Add("no_recoil",0,1,0,true,[]{return NoRecoil::enabled?1.:0.;},[](double x){return NoRecoil::Set(x!=0);});
 Add("full_auto",0,1,0,true,[]{return weaponFullAutoEnabled?1.:0.;},[](double x){SetWeaponFullAuto(x!=0);return true;});
 Add("fire_interval",0,1,0,true,[]{return weaponIntervalEnabled?1.:0.;},[](double x){SetWeaponInterval(x!=0);return true;});
                                                                                            
 Add("interval_value",1,100,10,true,[]{return(double)weaponIntervalHundredths.load();},[](double x){bool on=weaponIntervalEnabled.load();if(on)RestoreWeaponInterval();weaponIntervalHundredths=(int)x;if(on)MaintainWeaponModifiers();return true;});
 Atom("cash_amount",customCash,-1000000000,1000000000,10000);
 Atom("blueprint_selection",blueprintSelection,0,BlueprintCount-1,0);
 Atom("spawn_category",spawnCategory,0,SpawnCategoryCount-1,0);Atom("spawn_selection",spawnSelection,0,SpawnCount-1,0);
 const LONG normalTheme[6]={(LONG)0xF2161024,(LONG)0xFFF3ECFF,(LONG)0xFFB784F5,(LONG)0xFF704CA2,(LONG)0xFF392451,(LONG)0xFFCEAAFF};
 for(int i=0;i<6;i++){auto n="theme_"+std::to_string(i);Add(n.c_str(),-2147483648.,2147483647.,normalTheme[i],true,[i]{return(double)themeColors[i];},[i](double x){themeColors[i]=(LONG)x;return true;});}
 auto ctrl=[](const char*n,volatile LONG*ptr,double lo,double hi,double normal){if(!ptr)return;Add(n,lo,hi,normal,true,[ptr]{return(double)*ptr;},[ptr](double x){*ptr=(LONG)x;return true;});};
 if(control){ctrl("esp_max_distance",&control->maxDistanceTenths,100,8000,4000);ctrl("esp_interval",&control->skeletonIntervalMs,4,50,8);ctrl("esp_style",&control->styleScaleHundredths,0,200,0);ctrl("zombie_color",&control->zombieArgb,-2147483648.,2147483647.,(LONG)0xEBFF2323);ctrl("friendly_color",&control->friendlyArgb,-2147483648.,2147483647.,(LONG)0xEB00DCFF);ctrl("hostile_color",&control->hostileArgb,-2147483648.,2147483647.,(LONG)0xEBFF8C1E);}
}
std::filesystem::path Directory(){if(!overrideDirectory.empty())return overrideDirectory;wchar_t local[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");return std::filesystem::path(local)/L"JentaSpecialEdition"/L"MenuConfigs";}
bool AtomicWrite(const std::filesystem::path&path,const std::string&data){auto temp=path;temp+=L".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)return false;out<<data;out.flush();bool ok=!!out;out.close();return ok&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
std::string KeysText(){std::ostringstream out;for(int id=0;id<FeatureBindings::Capacity;id++)if(FeatureBindings::Exists(id))out<<"key_"<<id<<' '<<FeatureBindings::keys[id].load()<<'\n';return out.str();}
using Values=std::unordered_map<std::string,double>;
bool ReadFile(const std::filesystem::path&path,Values&values,const char*magic){std::error_code ec;auto size=std::filesystem::file_size(path,ec);if(ec||size>65536)return false;std::ifstream in(path);std::string line;if(!std::getline(in,line)||line!=magic)return false;while(std::getline(in,line)){std::istringstream row(line);std::string key,extra;double value;if(!(row>>key>>value)||row>>extra||!std::isfinite(value)||!values.emplace(key,value).second)return false;}return in.eof();}
                                                                     
                                                                           
bool UpgradeQuickBindings(Values&v,size_t expected){return v.size()==expected;}
bool UpgradePresetSchema(Values&v,size_t expected){return v.size()==expected;}
bool ValidateKeys(const Values&v){std::unordered_set<int>used;for(int id=0;id<FeatureBindings::Capacity;id++)if(FeatureBindings::Exists(id)){auto it=v.find("key_"+std::to_string(id));if(it==v.end()||it->second!=floor(it->second)||it->second<0||it->second>254)return false;int key=(int)it->second;if(!FeatureBindings::ValidKey(key)||(id==256&&!key)||(FeatureBindings::NativeOnly(id)&&key=='F')||(key&&!used.insert(key).second))return false;}return true;}
void ApplyKeys(const Values&v){FeatureBindings::armed=false;FeatureBindings::suppressUntilRelease=true;for(int id=0;id<FeatureBindings::Capacity;id++)if(FeatureBindings::Exists(id))FeatureBindings::keys[id]=(int)v.at("key_"+std::to_string(id));FeatureBindings::armed=false;FeatureBindings::suppressUntilRelease=true;hotkeys[3]=FeatureBindings::keys[256].load();}
void SaveBindings(){try{auto dir=Directory();std::filesystem::create_directories(dir);bool ok=AtomicWrite(dir/L"hotkeys.cfg","DIDE_HOTKEYS_V1\n"+KeysText());if(!ok)SetStatus("HOTKEY SAVE FAILED - CHECK LOG");RoutineDiagnostics::Emit(ok?"HOTKEYS_SAVED":"HOTKEYS_SAVE_FAILED");}catch(...){SetStatus("HOTKEY SAVE FAILED");}}
void LoadBindings(){FeatureBindings::Defaults();try{Values v;if(ReadFile(Directory()/L"hotkeys.cfg",v,"DIDE_HOTKEYS_V1")&&UpgradeQuickBindings(v,FeatureBindings::ActionCount())&&ValidateKeys(v))ApplyKeys(v);}catch(...){}hotkeys[3]=FeatureBindings::keys[256].load();}
bool Save(int slot){if(slot<1||slot>3){SetStatus("DEFAULT IS READ ONLY");return false;}try{Init();std::ostringstream out;out<<"DIDE_PRESET_V1\n"<<std::setprecision(17);for(auto&s:settings)out<<s.name<<' '<<s.get()<<'\n';out<<KeysText();auto dir=Directory();std::filesystem::create_directories(dir);bool ok=AtomicWrite(dir/("preset-"+std::to_string(slot)+".cfg"),out.str());SetStatus(ok?"PRESET SAVED":"PRESET SAVE FAILED");RoutineDiagnostics::Emit(ok?"PRESET_SAVED":"PRESET_SAVE_FAILED");return ok;}catch(...){SetStatus("PRESET SAVE FAILED");return false;}}
bool Load(int slot){if(slot<0||slot>3)return false;try{Init();Values v;if(!slot){for(auto&s:settings)v[s.name]=s.normal;for(int id=0;id<FeatureBindings::Capacity;id++)if(FeatureBindings::Exists(id))v["key_"+std::to_string(id)]=FeatureBindings::DefaultKey(id);}else if(!ReadFile(Directory()/("preset-"+std::to_string(slot)+".cfg"),v,"DIDE_PRESET_V1")){SetStatus("PRESET EMPTY OR INVALID - SAVE IT FIRST");return false;}
 if(!UpgradePresetSchema(v,settings.size()+FeatureBindings::ActionCount())||!ValidateKeys(v)){SetStatus("PRESET REJECTED - INVALID SCHEMA OR KEYS");return false;}
 for(auto&s:settings){auto i=v.find(s.name);if(i==v.end()||i->second<s.low||i->second>s.high||(s.integer&&i->second!=floor(i->second))){SetStatus("PRESET REJECTED - INVALID VALUE");return false;}}
 int category=(int)v.at("spawn_category"),selection=(int)v.at("spawn_selection");if(selection<SpawnCategoryStarts[category]||selection>=SpawnCategoryStarts[category]+SpawnCategoryCounts[category]){SetStatus("PRESET REJECTED - ITEM CATEGORY MISMATCH");return false;}
                                                       
 Spinbot::Stop();CameraLockOn::Clear();
                                                                          
 if(!slot){ResetFunFeatures();CancelWaveNative();}
 unsigned failed=0;for(auto&s:settings)if(!s.set(v.at(s.name))){++failed;RoutineDiagnostics::Emit(("PRESET_SETTING_UNAVAILABLE name="+s.name).c_str());}
 ApplyKeys(v);SaveBindings();SaveNativeSettings();
 SetStatus(failed?"PRESET PARTIAL - UNAVAILABLE FEATURES IN LOG":slot?"PRESET LOADED":"DEFAULT LOADED - GAMEPLAY FEATURES OFF");RoutineDiagnostics::Emit(failed?"PRESET_APPLY_PARTIAL":"PRESET_APPLIED");return !failed;
 }catch(...){SetStatus("PRESET LOAD FAILED - CHECK LOG");RoutineDiagnostics::Emit("PRESET_LOAD_EXCEPTION");return false;}}
}
void HandleBindingCapture(){
 int id=captureHotkey.load();if(id<0)return;
 if(!FeatureBindings::captureReleased){bool down=false;for(int k=1;k<255;k++)down|=(GetAsyncKeyState(k)&0x8000)!=0;if(!down)FeatureBindings::captureReleased=true;return;}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000){captureHotkey=-1;SetStatus("HOTKEY CAPTURE CANCELLED");return;}
 for(int k=1;k<255;k++)if(GetAsyncKeyState(k)&0x8000){if(!FeatureBindings::Assign(id,k)){SetStatus("KEY RESERVED OR ALREADY ASSIGNED");FeatureBindings::captureReleased=false;return;}captureHotkey=-1;FeatureBindings::suppressUntilRelease=true;hotkeys[3]=FeatureBindings::keys[256].load();MenuPresets::SaveBindings();SetStatus("HOTKEY SAVED");return;}
}
void BindingAction(int row,bool decrease){if(!row){FeatureBindings::group=(FeatureBindings::group.load()+(decrease?15:1))%16;menuIndex=0;return;}int id=FeatureBindings::Page()*16+row-1;if(!FeatureBindings::Exists(id))return;
 if(decrease){int key=id==256?VK_INSERT:0;if(!FeatureBindings::Assign(id,key)){SetStatus("MENU KEY RESET CONFLICT - UNBIND INSERT FIRST");return;}hotkeys[3]=FeatureBindings::keys[256].load();MenuPresets::SaveBindings();SetStatus("HOTKEY RESET");}else{captureHotkey=id;FeatureBindings::captureReleased=false;SetStatus("RELEASE KEYS THEN PRESS KEY - ESC CANCELS");}}
