# XPP HTTP API Reference

This document describes all available HTTP endpoints in the XPP backend system.

## Base URL

```
http://localhost:50051
```

## Authentication

Most endpoints require JWT authentication. Include the token in the Authorization header:

```
Authorization: Bearer <your_jwt_token>
```

---

## Health & Status

### GET /health

Health check endpoint to verify server status.

**Request:**
```http
GET /health HTTP/1.1
Host: localhost:50051
```

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "ok",
    "timestamp": 1737438000000
  }
}
```

**Status Codes:**
- `200 OK` - Server is healthy

---

### GET /

Root endpoint returning API information.

**Request:**
```http
GET / HTTP/1.1
Host: localhost:50051
```

**Response:**
```json
{
  "message": "XPP WeChat Backend API",
  "version": "1.0.0"
}
```

**Status Codes:**
- `200 OK` - Success

---

## Authentication Endpoints

### POST /api/auth/register

Register a new user account.

**Request:**
```http
POST /api/auth/register HTTP/1.1
Host: localhost:50051
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123",
  "email": "test@example.com"
}
```

**Request Body:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| username | string | Yes | Username (unique) |
| password | string | Yes | Password (min 6 characters) |
| email | string | Yes | Email address (must contain @) |

**Response:**
```json
{
  "success": true,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com",
      "is_active": true,
      "created_at": 1737438000
    }
  }
}
```

**Status Codes:**
- `200 OK` - Registration successful
- `400 Bad Request` - Invalid input or username already exists

**Validation Rules:**
- Username must not be empty
- Password must be at least 6 characters
- Email must contain '@' symbol

---

### POST /api/auth/login

Authenticate user and receive JWT token.

**Request:**
```http
POST /api/auth/login HTTP/1.1
Host: localhost:50051
Content-Type: application/json

{
  "username": "testuser",
  "password": "password123"
}
```

**Request Body:**
| Field | Type | Required | Description |
|-------|------|----------|-------------|
| username | string | Yes | Username |
| password | string | Yes | Password |

**Response:**
```json
{
  "success": true,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "user": {
      "id": 1,
      "username": "testuser",
      "email": "test@example.com",
      "avatar_url": "",
      "is_active": true
    }
  }
}
```

**Status Codes:**
- `200 OK` - Login successful
- `401 Unauthorized` - Invalid credentials

---

### GET /api/auth/me

Get current authenticated user information.

**Request:**
```http
GET /api/auth/me HTTP/1.1
Host: localhost:50051
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": 1,
    "username": "testuser",
    "email": "test@example.com",
    "avatar_url": "",
    "is_active": true
  }
}
```

**Status Codes:**
- `200 OK` - Success
- `401 Unauthorized` - Invalid or missing token

---

### POST /api/auth/logout

Logout current user and invalidate session.

**Request:**
```http
POST /api/auth/logout HTTP/1.1
Host: localhost:50051
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

**Response:**
```json
{
  "success": true,
  "data": {
    "message": "Logged out successfully"
  }
}
```

**Status Codes:**
- `200 OK` - Logout successful
- `401 Unauthorized` - Invalid or missing token

---

## Error Responses

All error responses follow this format:

```json
{
  "success": false,
  "error": "Error message description"
}
```

### Common Error Codes

| Status Code | Description |
|-------------|-------------|
| 400 Bad Request | Invalid request parameters or validation failed |
| 401 Unauthorized | Missing or invalid authentication token |
| 404 Not Found | Requested resource not found |
| 500 Internal Server Error | Server error occurred |

---

## Rate Limiting

Currently no rate limiting is implemented. This may be added in future versions.

---

## CORS

CORS is enabled by default for all origins when `server.enable_cors` is set to `true` in config.yaml.

**Allowed Methods:** GET, POST, PUT, DELETE, OPTIONS
**Allowed Headers:** Content-Type, Authorization

---

## Testing

See `tests/api_tests.cpp` for integration tests covering all endpoints.

Run tests with:
```bash
./build/Release/api_tests.exe
```

---

## Notes

- All timestamps are Unix timestamps in seconds
- JWT tokens expire after 24 hours
- Session data is stored in memory cache (lost on server restart)
- For production use, consider switching to Redis for persistent sessions
