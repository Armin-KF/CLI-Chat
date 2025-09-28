import React, { useState, useRef, useEffect } from 'react';
import {
  Box,
  AppBar,
  Toolbar,
  Typography,
  Button,
  Paper,
  TextField,
  List,
  ListItem,
  ListItemText,
  Chip,
  IconButton,
  Dialog,
  DialogTitle,
  DialogContent,
  DialogActions,
  Drawer,
  Badge,
} from '@mui/material';
import {
  Send as SendIcon,
  ExitToApp as LogoutIcon,
  People as PeopleIcon,
  Message as MessageIcon,
} from '@mui/icons-material';
import { format } from 'date-fns';
import { useAuth } from '../contexts/AuthContext';
import { useSocket } from '../contexts/SocketContext';

const Chat: React.FC = () => {
  const [message, setMessage] = useState('');
  const [privateRecipient, setPrivateRecipient] = useState('');
  const [showUsers, setShowUsers] = useState(false);
  const [privateDialog, setPrivateDialog] = useState(false);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const { user, logout } = useAuth();
  const { messages, onlineUsers, isConnected, sendMessage, sendPrivateMessage } = useSocket();

  useEffect(() => {
    scrollToBottom();
  }, [messages]);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  };

  const handleSendMessage = (e: React.FormEvent) => {
    e.preventDefault();
    if (message.trim() && isConnected) {
      sendMessage(message.trim());
      setMessage('');
    }
  };

  const handleSendPrivateMessage = () => {
    if (message.trim() && privateRecipient && isConnected) {
      sendPrivateMessage(privateRecipient, message.trim());
      setMessage('');
      setPrivateDialog(false);
      setPrivateRecipient('');
    }
  };

  const startPrivateMessage = (username: string) => {
    setPrivateRecipient(username);
    setPrivateDialog(true);
    setShowUsers(false);
  };

  return (
    <Box sx={{ display: 'flex', flexDirection: 'column', height: '100vh' }}>
      {/* Header */}
      <AppBar position="static">
        <Toolbar>
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            CLI-Chat - Welcome, {user?.username}
          </Typography>
          <Chip
            icon={<Badge color="success" variant="dot" />}
            label={isConnected ? 'Connected' : 'Disconnected'}
            color={isConnected ? 'success' : 'error'}
            sx={{ mr: 2 }}
          />
          <IconButton
            color="inherit"
            onClick={() => setShowUsers(true)}
            sx={{ mr: 1 }}
          >
            <Badge badgeContent={onlineUsers.length} color="secondary">
              <PeopleIcon />
            </Badge>
          </IconButton>
          <Button
            color="inherit"
            startIcon={<LogoutIcon />}
            onClick={logout}
          >
            Logout
          </Button>
        </Toolbar>
      </AppBar>

      {/* Messages Area */}
      <Box
        sx={{
          flexGrow: 1,
          overflow: 'auto',
          p: 1,
          backgroundColor: 'background.default',
        }}
      >
        <Paper sx={{ height: '100%', p: 2, overflow: 'auto' }}>
          <List sx={{ height: '100%', overflow: 'auto' }}>
            {messages.map((msg, index) => (
              <ListItem
                key={index}
                sx={{
                  flexDirection: 'column',
                  alignItems: 'flex-start',
                  borderLeft: msg.isPrivate ? '3px solid orange' : 'none',
                  backgroundColor: msg.isPrivate ? 'action.hover' : 'transparent',
                  mb: 1,
                  borderRadius: 1,
                }}
              >
                <Box sx={{ display: 'flex', alignItems: 'center', width: '100%' }}>
                  <Typography
                    variant="caption"
                    color="text.secondary"
                    sx={{ mr: 1 }}
                  >
                    {format(msg.timestamp, 'HH:mm:ss')}
                  </Typography>
                  <Typography
                    variant="subtitle2"
                    color={msg.username === 'System' ? 'warning.main' : 'primary.main'}
                    sx={{ fontWeight: 'bold' }}
                  >
                    {msg.username}
                    {msg.isPrivate && ' (private)'}:
                  </Typography>
                </Box>
                <ListItemText
                  primary={msg.content}
                  sx={{
                    mt: 0,
                    '& .MuiListItemText-primary': {
                      fontSize: '0.95rem',
                      fontFamily: 'monospace',
                    },
                  }}
                />
              </ListItem>
            ))}
            <div ref={messagesEndRef} />
          </List>
        </Paper>
      </Box>

      {/* Message Input */}
      <Paper
        component="form"
        onSubmit={handleSendMessage}
        sx={{
          p: 2,
          display: 'flex',
          alignItems: 'center',
          gap: 1,
        }}
      >
        <TextField
          fullWidth
          variant="outlined"
          placeholder="Type your message..."
          value={message}
          onChange={(e) => setMessage(e.target.value)}
          disabled={!isConnected}
          inputProps={{ maxLength: 4096 }}
        />
        <IconButton
          type="submit"
          color="primary"
          disabled={!message.trim() || !isConnected}
        >
          <SendIcon />
        </IconButton>
        <Button
          variant="outlined"
          startIcon={<MessageIcon />}
          onClick={() => setPrivateDialog(true)}
          disabled={!isConnected || onlineUsers.length === 0}
        >
          Private
        </Button>
      </Paper>

      {/* Online Users Drawer */}
      <Drawer
        anchor="right"
        open={showUsers}
        onClose={() => setShowUsers(false)}
      >
        <Box sx={{ width: 300, p: 2 }}>
          <Typography variant="h6" gutterBottom>
            Online Users ({onlineUsers.length})
          </Typography>
          <List>
            {onlineUsers.map((username) => (
              <ListItem
                key={username}
                button
                onClick={() => startPrivateMessage(username)}
              >
                <Badge color="success" variant="dot" sx={{ mr: 2 }}>
                  <PeopleIcon />
                </Badge>
                <ListItemText primary={username} />
              </ListItem>
            ))}
            {onlineUsers.length === 0 && (
              <ListItem>
                <ListItemText primary="No other users online" />
              </ListItem>
            )}
          </List>
        </Box>
      </Drawer>

      {/* Private Message Dialog */}
      <Dialog
        open={privateDialog}
        onClose={() => setPrivateDialog(false)}
        maxWidth="sm"
        fullWidth
      >
        <DialogTitle>Send Private Message</DialogTitle>
        <DialogContent>
          <TextField
            autoFocus
            margin="dense"
            label="Recipient"
            select
            fullWidth
            variant="outlined"
            value={privateRecipient}
            onChange={(e) => setPrivateRecipient(e.target.value)}
            SelectProps={{ native: true }}
            sx={{ mb: 2 }}
          >
            <option value="">Select a user...</option>
            {onlineUsers.map((username) => (
              <option key={username} value={username}>
                {username}
              </option>
            ))}
          </TextField>
          <TextField
            margin="dense"
            label="Message"
            fullWidth
            multiline
            rows={3}
            variant="outlined"
            value={message}
            onChange={(e) => setMessage(e.target.value)}
            inputProps={{ maxLength: 4096 }}
          />
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPrivateDialog(false)}>Cancel</Button>
          <Button
            onClick={handleSendPrivateMessage}
            variant="contained"
            disabled={!message.trim() || !privateRecipient}
          >
            Send
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
};

export default Chat;