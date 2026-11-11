/* Shared transient snapshots, single writer per slot. Page-file backed, never
 * mapped onto a game/save file. Versioned name + per-launch random token.
 * Readers attempt once: an in-progress or changed write keeps the old snapshot.
 * Interlocked operations are full barriers on supported Windows x64.
 * Event counters only notify about durable journals; they are NOT an ACK.
 */
#define MAIL_SLOTS 11
#define MAIL_LIMIT (2*1024*1024)
#define RING_COUNT 512
#define RING_LIMIT 4096
typedef struct {
 volatile LONG64 revision;
 ULONGLONG written;
 DWORD length;
 unsigned char reserved[44];
 char data[MAIL_LIMIT];
} MailSlot;
typedef struct { DWORD length; char data[RING_LIMIT]; } MailRecord;
typedef struct { volatile LONG64 head; char pad1[56]; volatile LONG64 tail; char pad2[56]; MailRecord record[RING_COUNT]; } MailRing;
typedef struct { volatile LONG64 events[2]; char reserved[48]; MailSlot slot[MAIL_SLOTS]; MailRing ring[2]; } MailData;
typedef struct { HANDLE mapping,ready[2]; MailData *data; } Mail;
static const char *mailNames[MAIL_SLOTS]={"out.state","in.state","out.visual","in.visual","out.enemy","in.enemy","out.roster","in.roster","out.world","in.world","status.txt"};
static int mailindex(const char *name){for(int i=0;i<MAIL_SLOTS;i++)if(!strcmp(name,mailNames[i]))return i;return -1;}
__declspec(dllexport) void *qdmail_open(const wchar_t *name){
 if(!name||wcsncmp(name,L"Local\\QDCoopIPC2_",17)||wcslen(name)!=49)return NULL;
 for(int i=17;i<49;i++)if(!((name[i]>=L'0'&&name[i]<=L'9')||(name[i]>=L'a'&&name[i]<=L'f')))return NULL;
 Mail *m=calloc(1,sizeof(Mail));if(!m)return NULL;
 m->mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(MailData),name);
 if(m->mapping)m->data=MapViewOfFile(m->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(MailData));
 if(m->data)for(int i=0;i<2;i++){wchar_t eventName[64];swprintf(eventName,64,L"%ls_%d",name,i);m->ready[i]=CreateEventW(NULL,FALSE,FALSE,eventName);}
 if(!m->data||!m->ready[0]||!m->ready[1]){if(m->data)UnmapViewOfFile(m->data);if(m->mapping)CloseHandle(m->mapping);for(int i=0;i<2;i++)if(m->ready[i])CloseHandle(m->ready[i]);free(m);return NULL;}return m;
}
__declspec(dllexport) void qdmail_close(void *handle){Mail *m=handle;if(!m)return;UnmapViewOfFile(m->data);CloseHandle(m->mapping);for(int i=0;i<2;i++)CloseHandle(m->ready[i]);free(m);}
/* side0 = bridge, side1 = game worker. Only background threads wait. */
__declspec(dllexport) HANDLE qdmail_ready(void *handle,int side){Mail*m=handle;return m&&side>=0&&side<2?m->ready[side]:NULL;}
__declspec(dllexport) int qdmail_write(void *handle,int id,const char *value,int length){
 Mail *m=handle;if(!m||id<0||id>=MAIL_SLOTS||length<0||length>MAIL_LIMIT||(!value&&length))return 0;
 MailSlot *s=&m->data->slot[id];
 /* Single writer, including replacement bridge after the old process exits.
    Recover a revision left odd by a terminated writer without spinning. */
 LONG64 rev=(InterlockedCompareExchange64(&s->revision,0,0)&~1LL)+1;
 InterlockedExchange64(&s->revision,rev);
 if(length)memcpy(s->data,value,(size_t)length);
 s->length=(DWORD)length;s->written=GetTickCount64();
 InterlockedExchange64(&s->revision,rev+1);SetEvent(m->ready[id==10?1:(id&1)]);return 1;
}
__declspec(dllexport) int qdmail_read(void *handle,int id,char *value,int capacity,ULONGLONG *revision,int *length,ULONGLONG *written){
 Mail *m=handle;if(!m||id<0||id>=MAIL_SLOTS||!value||!revision||!length||!written)return -1;
 MailSlot *s=&m->data->slot[id];ULONGLONG rev=(ULONGLONG)InterlockedCompareExchange64(&s->revision,0,0);
 if(!rev||(rev&1)||rev==*revision)return 0;
 DWORD n=s->length;ULONGLONG when=s->written;if(n>MAIL_LIMIT||n>(DWORD)capacity)return -1;
 if(n)memcpy(value,s->data,n);
 if(rev!=(ULONGLONG)InterlockedCompareExchange64(&s->revision,0,0))return 0;
 *revision=rev;*length=(int)n;*written=when;return 1;
}
__declspec(dllexport) ULONGLONG qdmail_event(void *handle,int id,int increment){
 Mail *m=handle;if(!m||id<0||id>1)return 0;
 ULONGLONG value=(ULONGLONG)(increment?InterlockedIncrement64(&m->data->events[id]):InterlockedCompareExchange64(&m->data->events[id],0,0));if(increment)SetEvent(m->ready[id]);return value;
}
/* SPSC per direction. Full queues retain their producer data; never overwrite an
   unread event. Producer publishes only after the record is complete. Consumers
   advance only after copying. Named IPC2 prevents layout mixing with IPC1. */
__declspec(dllexport) int qdmail_push(void *handle,int id,const char *value,int length){
 Mail*m=handle;if(!m||id<0||id>1||length<1||length>RING_LIMIT||!value)return 0;
 MailRing*r=&m->data->ring[id];ULONGLONG head=(ULONGLONG)InterlockedCompareExchange64(&r->head,0,0),tail=(ULONGLONG)InterlockedCompareExchange64(&r->tail,0,0);
 if(head-tail>=RING_COUNT)return 0;
 MailRecord*v=&r->record[head%RING_COUNT];memcpy(v->data,value,(size_t)length);v->length=(DWORD)length;
 InterlockedExchange64(&r->head,(LONG64)(head+1));SetEvent(m->ready[id]);return 1;
}
__declspec(dllexport) int qdmail_drain(void *handle,int id,char *value,int capacity,int maxRecords){
 Mail*m=handle;if(!m||id<0||id>1||!value||capacity<1||maxRecords<1)return 0;
 MailRing*r=&m->data->ring[id];ULONGLONG head=(ULONGLONG)InterlockedCompareExchange64(&r->head,0,0),tail=(ULONGLONG)InterlockedCompareExchange64(&r->tail,0,0);int n=0;
 for(int count=0;tail<head&&count<maxRecords;tail++,count++){
  MailRecord*v=&r->record[tail%RING_COUNT];DWORD len=v->length;if(len>RING_LIMIT||len>(DWORD)(capacity-n))break;
  memcpy(value+n,v->data,len);n+=(int)len;
 }
 InterlockedExchange64(&r->tail,(LONG64)tail);return n;
}
__declspec(dllexport) void qdmail_discard_events(void *handle,int id){
 Mail*m=handle;if(!m||id<0||id>1)return;MailRing*r=&m->data->ring[id];
 InterlockedExchange64(&r->tail,InterlockedCompareExchange64(&r->head,0,0));
}
__declspec(dllexport) int qdmail_pending(void *handle,int id){Mail*m=handle;if(!m||id<0||id>1)return 0;MailRing*r=&m->data->ring[id];return InterlockedCompareExchange64(&r->head,0,0)!=InterlockedCompareExchange64(&r->tail,0,0);}
__declspec(dllexport) unsigned qdmail_changed(void *handle,const ULONGLONG*revision,int count){Mail*m=handle;if(!m||!revision||count!=MAIL_SLOTS)return 0;unsigned mask=0;for(int i=0;i<count;i++){ULONGLONG v=(ULONGLONG)InterlockedCompareExchange64(&m->data->slot[i].revision,0,0);if(v&&!(v&1)&&v!=revision[i])mask|=1u<<i;}return mask;}
