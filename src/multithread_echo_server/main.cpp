#include <WinSock2.h>
#include <iostream>
#include <future>
#include <deque>

int main()
{
    //#윈속 초기화 (WAS, Windows Socket Api -> 윈속 라이브러리 초기화해서 로딩해달라 (파라미터로 버전 쓰는데 2,2 쓰며됨))
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
                //수신한 문자열을 그대로 반향(Echo)전송.
                ::send(hClient, szBuffer, sizeof(szBuffer), 0);
                std::cout << szBuffer << std::endl;
                memset(szBuffer, 0, sizeof(szBuffer));
            }

            std::cout << "클라이언트 연결이 끊겼습니다.\n";
            ::closesocket(hClient);
        }));
    }

    //5. 리슨 소켓 닫기
    ::closesocket(hSocket);

    //#윈속 해제
    ::WSACleanup();
    return 0;
}
