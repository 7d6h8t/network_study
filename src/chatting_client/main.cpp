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

    //클라이언트 소켓 생성
    SOCKET hSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (hSocket == INVALID_SOCKET)
    {
        std::cout << "ERROR: 소켓을 생성할 수 없습니다.\n";
        return 0;
    }

    //포트 바인딩 및 연결
    SOCKADDR_IN svraddr = { 0 };
    svraddr.sin_family = AF_INET;
    svraddr.sin_port = htons(25000);
    svraddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    if (::connect(hSocket, (SOCKADDR*)&svraddr, sizeof(svraddr)) == SOCKET_ERROR)
    {
        std::cout << "ERROR: 서버에 연결할 수 없습니다.\n";
        return 0;
    }

    std::deque<std::future<void>> task_queue;
    //채팅 메시지 수신 스레드 생성
    task_queue.push_back(std::async(std::launch::async, [&, hSocket] {
        //서버가 보낸 메시지를 수신하고 화면에 출력하는 스레드 함수.
        char szBuffer[128] = { 0 };
        while (::recv(hSocket, szBuffer, sizeof(szBuffer), 0) > 0)
        {
            std::cout << "-> " << szBuffer << std::endl;
            memset(szBuffer, 0, sizeof(szBuffer));
        }

        std::cout << "수신 스레드가 끝났습니다.\n";
    }));

    //채팅 메시지 송신
    char szBuffer[128] = { 0 };
    while (1)
    {
        //사용자로부터 문자열을 입력 받는다.
        std::cin >> szBuffer;
        if (strcmp(szBuffer, "EXIT") == 0)
            break;

        //사용자가 입력한 문자열을 서버에 전송한다.
        ::send(hSocket, szBuffer, (int)strlen(szBuffer) + 1, 0);
    }

    //소켓을 닫고 종료.
    ::closesocket(hSocket);
    //스레드가 종료될 수 있도록 잠시 기다려준다.
    ::Sleep(100);
    //#윈속 해제
    ::WSACleanup();
    return 0;
}
