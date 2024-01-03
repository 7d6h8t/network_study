#include <WinSock2.h>
#include <iostream>
#include <future>
#include <deque>
#include <list>
#include <mutex>

std::mutex  g_mutex;                //스레드 동기화 객체.
std::list<SOCKET>   g_listClient;   //연결된 클라이언트 소켓 리스트.

//새로 연결된 클라이언트의 소켓을 리스트에 저장한다.
BOOL AddUser(SOCKET hSocket)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_listClient.push_back(hSocket);

    return TRUE;
}

//연결된 클라이언트 모두에게 메시지를 전송한다.
void SendChattingMessage(char *pszParam)
{
    int nLength = (int)strlen(pszParam);

    std::lock_guard<std::mutex> lock(g_mutex);
    //연결된 모든 클라이언트들에게 같은 메시지를 전달한다.
    for (SOCKET hClient : g_listClient)
        ::send(hClient, pszParam, sizeof(char) * (nLength + 1), 0);

}

int main()
{
    //#윈속 초기화 (WAS, Windows Socket Api -> 윈속 라이브러리 초기화해서 로딩해달라 (파라미터로 버전 쓰는데 2,2 쓰면됨))
    WSADATA wsa = { 0 };
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "ERROR: 윈속을 초기화 할 수 없습니다.\n";
        return 0;
    }

    //1. 접속대기 소켓 생성
    SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (hSocket == INVALID_SOCKET)
    {
        std::cout << "ERROR: 접속 대기 소켓을 생성할 수 없습니다.\n";
        return 0;
    }

    //2. 포트 바인딩
    SOCKADDR_IN svraddr = { 0 };
    svraddr.sin_family = AF_INET;
    svraddr.sin_port = htons(25000);
    svraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    if (::bind(hSocket, (SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
    {
        std::cout << "ERROR: 소켓에 IP주소와 포트를 바인드 할 수 없습니다.\n";
        return 0;
    }

    //3. 접속대기 상태로 전환
    if (::listen(hSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "ERROR: 리슨 상태로 전환할 수 없습니다.\n";
        return 0;
    }

    std::cout << "*** 채팅서버를 시작합니다. ***\n";

    //4. 클라이언트 접속 처리 및 대응
    SOCKADDR_IN clientaddr = { 0 };
    int nAddrLen = sizeof(clientaddr);
    SOCKET hClient = 0;
    std::deque<std::future<void>> task_queue;

    //4.1 클라이언트 연결을 받아들이고 새로운 소켓 생성(개방)
    while ((hClient = ::accept(hSocket,
        (SOCKADDR*)&clientaddr,
        &nAddrLen)) != INVALID_SOCKET)
    {
        if (AddUser(hClient) == FALSE)
        {
            std::cout << "ERROR: 더 이상 클라이언트 연결을 처리할 수 없습니다.\n";
            break;
        }

        for (auto itr = task_queue.begin(); itr != task_queue.end();)
        {
            if (itr->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                itr = task_queue.erase(itr);
            else
                ++itr;
        }

        //4.2 새 클라이언트와 통신하기 위한 스레드 생성
        //클라이언트 마다 스레드가 하나씩 생성된다.
        task_queue.push_back(std::async(std::launch::async, [&, hClient] {
            char szBuffer[128] = { 0 };
            int nRecevie = 0;

            std::cout << "새 클라이언트가 연결되었습니다.\n";

            //클라이언트로부터 문자열을 수신한다.
            while ((nRecevie = ::recv(hClient, szBuffer, sizeof(szBuffer), 0)) > 0)
            {
                std::cout << szBuffer << std::endl;
                //수신한 문자열을 연결된 전체 클라이언트들에게 전송
                SendChattingMessage(szBuffer);
                memset(szBuffer, 0, sizeof(szBuffer));
            }

            std::cout << "클라이언트 연결이 끊겼습니다.\n";

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_listClient.remove(hClient);
                ::closesocket(hClient);
            }
        }));
    }

    //연결된 모든 클라이언트 및 리슨 소켓을 닫고 프로그램을 종료한다.
    ::shutdown(hSocket, SD_BOTH);

    {
        std::lock_guard<std::mutex> lock(g_mutex);

        for (SOCKET hClient : g_listClient)
            ::closesocket(hClient);

        //연결 리스트에 등록된 모든 정보를 삭제한다.
        g_listClient.clear();
    }

    std::cout << "모든 클라이언트 연결을 종료했습니다.\n";
    //클라이언트와 통신하는 스레드들이 종료되기를 기다린다.
    ::Sleep(100);

    //5. 리슨 소켓 닫기
    ::closesocket(hSocket);

    //#윈속 해제
    ::WSACleanup();

    std::cout << "*** 채팅서버를 종료합니다. ***\n";
    return 0;
}
