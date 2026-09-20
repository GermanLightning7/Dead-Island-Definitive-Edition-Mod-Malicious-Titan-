                                                                                   
namespace CashEdit {
template<class Reader,class Writer> bool Apply(uintptr_t stats,int amount,Reader read,Writer write,bool& rollbackOk){
 const uintptr_t offsets[3]={0xBC8,0x1C30,0xBD0};int before[3]{},after[3]{};rollbackOk=true;
 for(int i=0;i<3;++i){if(!read(stats+offsets[i],before[i]))return false;long long value=(long long)before[i]+amount;if(value<INT_MIN||value>INT_MAX)return false;after[i]=(int)value;}
 int changed=0;bool ok=true;
 for(int i=0;i<3;++i){int current=0;if(!read(stats+offsets[i],current)||current!=before[i]){ok=false;break;}changed=i+1;if(!write(stats+offsets[i],after[i])||!read(stats+offsets[i],current)||current!=after[i]){ok=false;break;}}
 if(ok)return true;
 for(int i=changed-1;i>=0;--i){int current=0;if(!read(stats+offsets[i],current)){rollbackOk=false;continue;}if(current==before[i])continue;if(current!=after[i]||!write(stats+offsets[i],before[i])||!read(stats+offsets[i],current)||current!=before[i])rollbackOk=false;}
 return false;
}
}
