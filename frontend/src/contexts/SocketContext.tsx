import React, { createContext, useContext, useEffect, useState, ReactNode } from 'react';
import { io, Socket } from 'socket.io-client';
import { useAuth } from './AuthContext';

interface Message {
  id?: number;
  username: string;
  content: string;
  timestamp: Date;
  isPrivate?: boolean;
  recipient?: string;
}

interface SocketContextType {
  socket: Socket | null;
  messages: Message[];
  onlineUsers: string[];
  isConnected: boolean;
  sendMessage: (content: string) => void;
  sendPrivateMessage: (recipient: string, content: string) => void;
}

const SocketContext = createContext<SocketContextType | undefined>(undefined);

export const useSocket = (): SocketContextType => {
  const context = useContext(SocketContext);
  if (!context) {
    throw new Error('useSocket must be used within a SocketProvider');
  }
  return context;
};

interface SocketProviderProps {
  children: ReactNode;
}

export const SocketProvider: React.FC<SocketProviderProps> = ({ children }) => {
  const [socket, setSocket] = useState<Socket | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [onlineUsers, setOnlineUsers] = useState<string[]>([]);
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const { user, token, isAuthenticated } = useAuth();

  useEffect(() => {
    if (isAuthenticated && user && token) {
      // Connect to WebSocket
      const newSocket = io('/', {
        auth: {
          token,
        },
        transports: ['websocket'],
      });

      newSocket.on('connect', () => {
        console.log('Connected to server');
        setIsConnected(true);
      });

      newSocket.on('disconnect', () => {
        console.log('Disconnected from server');
        setIsConnected(false);
      });

      newSocket.on('message', (data: any) => {
        const message: Message = {
          username: data.username,
          content: data.content,
          timestamp: new Date(data.timestamp),
          isPrivate: data.isPrivate || false,
          recipient: data.recipient,
        };
        setMessages(prev => [...prev, message]);
      });

      newSocket.on('private_message', (data: any) => {
        const message: Message = {
          username: data.sender,
          content: data.content,
          timestamp: new Date(data.timestamp),
          isPrivate: true,
        };
        setMessages(prev => [...prev, message]);
      });

      newSocket.on('user_list', (users: string[]) => {
        setOnlineUsers(users.filter(u => u !== user.username));
      });

      newSocket.on('system_message', (data: any) => {
        const message: Message = {
          username: 'System',
          content: data.message,
          timestamp: new Date(),
        };
        setMessages(prev => [...prev, message]);
      });

      setSocket(newSocket);

      return () => {
        newSocket.close();
      };
    } else {
      // Cleanup when not authenticated
      if (socket) {
        socket.close();
        setSocket(null);
      }
      setMessages([]);
      setOnlineUsers([]);
      setIsConnected(false);
    }
  }, [isAuthenticated, user, token]);

  const sendMessage = (content: string) => {
    if (socket && isConnected) {
      socket.emit('broadcast_message', { content });
    }
  };

  const sendPrivateMessage = (recipient: string, content: string) => {
    if (socket && isConnected) {
      socket.emit('private_message', { recipient, content });
    }
  };

  const value: SocketContextType = {
    socket,
    messages,
    onlineUsers,
    isConnected,
    sendMessage,
    sendPrivateMessage,
  };

  return <SocketContext.Provider value={value}>{children}</SocketContext.Provider>;
};