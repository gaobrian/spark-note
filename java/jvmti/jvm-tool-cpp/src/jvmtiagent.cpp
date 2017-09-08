#include <iostream>
#include <string.h>

#include "jvmtiagent.h"
#include "jvmti.h"

using namespace std;

jvmtiEnv* JvmTIAgent::m_jvmti = 0;
char* JvmTIAgent::m_options = 0;

// ��Ŀ����ǰ׺
const char * g_SelfJavaPackageName = "com/borey";

JvmTIAgent::~JvmTIAgent() throw(AgentException)
{
    // �����ͷ��ڴ棬��ֹ�ڴ�й¶
    m_jvmti->Deallocate(reinterpret_cast<unsigned char*>(m_options));
}

void JvmTIAgent::Init(JavaVM *vm) const throw(AgentException){
    jvmtiEnv *jvmti = 0;
	jint ret = (vm)->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0);
	if (ret != JNI_OK || jvmti == 0) {
		throw AgentException(JVMTI_ERROR_INTERNAL);
	}
	m_jvmti = jvmti;
}

void JvmTIAgent::ParseOptions(const char* str) const throw(AgentException)
{
    if (str == 0)
        return;
	const size_t len = strlen(str);
	if (len == 0) 
		return;

  	// ���������ڴ渴�ƹ���
	jvmtiError error;
    error = m_jvmti->Allocate(len + 1,reinterpret_cast<unsigned char**>(&m_options));
	CheckException(error);
    strcpy(m_options, str);

    // ������������в��������Ĺ���
	// ...
	cout << "[JVMTI Agent] Application Options: " << m_options <<endl;
}

void JvmTIAgent::AddCapability() const throw(AgentException)
{
    // ����һ���µĻ���
    jvmtiCapabilities caps;
    memset(&caps, 0, sizeof(caps));
	// �������� ���������¼�
    caps.can_generate_method_entry_events = 1;
	// ���Զ�ÿ������Ҫ������ ClassFileLoadHook �¼�
    caps.can_generate_all_class_hook_events = 1;
	
    // ���õ�ǰ����
    jvmtiError error = m_jvmti->AddCapabilities(&caps);
	CheckException(error);
}
  
void JvmTIAgent::RegisterEvent() const throw(AgentException)
{
    // ����һ���µĻص�����
    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
	// ���÷������뺯��ָ��
    callbacks.MethodEntry = &JvmTIAgent::HandleMethodEntry;
    
    // ���ûص�����
    jvmtiError error;
    error = m_jvmti->SetEventCallbacks(&callbacks, static_cast<jint>(sizeof(callbacks)));
	CheckException(error);

	// �����¼�����
	error = m_jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, 0);
	CheckException(error);
}

void JNICALL JvmTIAgent::HandleMethodEntry(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID method)
{
	try {
        jvmtiError error;
        jclass clazz;
        char* name;
		char* signature;
        
		// ��÷�����Ӧ����
        error = m_jvmti->GetMethodDeclaringClass(method, &clazz);
        CheckException(error);
        // ������ǩ��
        error = m_jvmti->GetClassSignature(clazz, &signature, 0);
        CheckException(error);
        // ��÷�������
        error = m_jvmti->GetMethodName(method, &name, NULL, NULL);
        CheckException(error);
        
		cout << "[JVMTI Agent] Enter method" << signature << " -> " << name << "(..)"<< endl;

        // �����ͷ��ڴ棬�����ڴ�й¶
        error = m_jvmti->Deallocate(reinterpret_cast<unsigned char*>(name));
		CheckException(error);
        error = m_jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
		CheckException(error);

	} catch (AgentException& e) {
		cerr << "Error when enter HandleMethodEntry: " << e.what() << " [" << e.ErrCode() << "]";
    }
}

void JNICALL JvmTIAgent::HandleClassFileLoadHook(
	jvmtiEnv *jvmti_env, 
	JNIEnv* jni_env, 
	jclass class_being_redefined, 
	jobject loader, 
	const char* name, 
	jobject protection_domain,
    jint class_data_len, 
	const unsigned char* class_data, 
	jint* new_class_data_len, 
	unsigned char** new_class_data)
{
	try{
		// class �ļ����ȸ�ֵ
		*new_class_data_len = class_data_len;
		jvmtiError error;
		// ���� �µ�class �ַ��ռ�
		error = m_jvmti->Allocate(class_data_len, new_class_data);
		CheckException(error);
		unsigned char *pNewClass = *new_class_data;
		unsigned int selfPackageLen = strlen(g_SelfJavaPackageName);
		if (strlen(name) > selfPackageLen && strncmp(name, g_SelfJavaPackageName, selfPackageLen)){
			// ���� class ���� ������������� 0x07, ż�� ��� 0x08, ����λ�� ���һλ�������0x09��
			int index = 0;
			for(; index < class_data_len-1 ; ){
				*pNewClass++ = class_data[index+1] ^ 0x08;
				*pNewClass++ = class_data[index]   ^ 0x07;
				index += 2; 
			}
			
			if ( 0 == index  && 1 == class_data_len){	// size 1
				*pNewClass = class_data[index] ^ 0x09;
			}else if ( class_data_len-1 == index ){ 	// size 2n + 1
				*pNewClass = class_data[index + 1] ^ 0x09;
			}
			cout << "[JVMTI Agent] Decrypt class (" << name << ") finished !" <<endl;
		}else{
			memcpy(pNewClass, class_data, class_data_len);
		}
		
	}catch(AgentException& e) {
		cerr << "Error when enter HandleClassFileLoadHook: " << e.what() << " [" << e.ErrCode() << "]";
	}	 
}