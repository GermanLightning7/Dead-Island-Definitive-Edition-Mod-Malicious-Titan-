void ReadableWeaponLabel(const char*id,char(&out)[96]){
 if(!strcmp(id,"Craftplan_GrapplingHook")){strcpy_s(out,"GRAPPLING HOOK");return;}
 if(!strcmp(id,"Craftplan_Stk2")){strcpy_s(out,"CHAOS HOMERUN - RANDOM ELEMENTS");return;}
 if(!strcmp(id,"OPFX")){strcpy_s(out,"CHAOS FISTS - RANDOM ELEMENTS");return;}
 if(!strcmp(id,"Craftplan_Stickybombcraft")){strcpy_s(out,"STICKY BOMB");return;}
 if(!strcmp(id,"Craftplan_Dev2")){strcpy_s(out,"DEV CRAFT 2 - HOMERUN");return;}
 if(!strcmp(id,"Medkit_HealthPackMedium")){strcpy_s(out,"MEDIUM MEDKIT");return;}
 if(!strcmp(id,"Medkit_HealthPackLarge")){strcpy_s(out,"LARGE MEDKIT");return;}
 const char*text=id;
 if(!strncmp(text,"zzz_",4))text+=4;
 for(const char*prefix:{"Craftplan_","Firearm_","Melee_","CraftPart_","Throwable_","Food_"}){size_t n=strlen(prefix);if(!strncmp(text,prefix,n)){text+=n;break;}}
 if(!strncmp(text,"leg_",4))text+=4;
 if(!strncmp(text,"Dev",3)&&text[3]>='0'&&text[3]<='9'){sprintf_s(out,"DEV CRAFT %s",text+3);return;}
 size_t length=strlen(text);if(length>3&&!strcmp(text+length-3,"Gen"))length-=3;
 int j=0;
 for(size_t i=0;i<length&&j<94;i++){
  char c=text[i];bool upper=c>='A'&&c<='Z';char previous=i?text[i-1]:0;
  if(upper&&j&&out[j-1]!=' '&&((previous>='a'&&previous<='z')||(previous>='0'&&previous<='9')))out[j++]=' ';
  if(c=='_')c=' ';if(c>='a'&&c<='z')c=char(c-'a'+'A');
  if(c!=' '||!j||out[j-1]!=' ')out[j++]=c;
 }
 while(j&&out[j-1]==' ')--j;out[j]=0;
}
