"""
Common utilities for Mini2 Python servers
Provides shared functionality for request management, cleanup, etc.
"""

import threading
import time
from datetime import datetime, timedelta
from abc import ABC, abstractmethod


class CleanupManager:
    """
    Common cleanup manager for expired requests/sessions
    """

    def __init__(self, expiry_minutes=30, cleanup_interval_minutes=5):
        self.expiry_time = timedelta(minutes=expiry_minutes)
        self.cleanup_interval = timedelta(minutes=cleanup_interval_minutes)
        self._running = True
        self._cleanup_thread = threading.Thread(target=self._cleanup_loop, daemon=True)

    def start(self):
        """Start the cleanup thread"""
        self._cleanup_thread.start()

    def stop(self):
        """Stop the cleanup thread"""
        self._running = False
        if self._cleanup_thread.is_alive():
            self._cleanup_thread.join()

    def _cleanup_loop(self):
        """Main cleanup loop"""
        while self._running:
            time.sleep(self.cleanup_interval.total_seconds())
            self._cleanup_expired_items()

    @abstractmethod
    def _cleanup_expired_items(self):
        """Override this to implement specific cleanup logic"""
        pass


class CommonUtils:
    """
    Common utility functions
    """

    _request_counter = 0
    _counter_lock = threading.Lock()

    @staticmethod
    def generate_request_id(prefix):
        """
        Generate unique request ID with server prefix
        Thread-safe implementation
        """
        with CommonUtils._counter_lock:
            CommonUtils._request_counter += 1
            return f"{prefix}_{CommonUtils._request_counter}"


class RequestMapping:
    """
    Common structure for leader servers to track request mappings to workers
    Can handle single worker (like Server B) or multiple workers (like Server D)
    """

    def __init__(self, worker_request_id=None, worker_e_request_id=None, worker_f_request_id=None):
        self.worker_request_id = worker_request_id  # For single worker scenarios
        self.worker_e_request_id = worker_e_request_id  # For Server D - Worker E
        self.worker_f_request_id = worker_f_request_id  # For Server D - Worker F
        self.last_access = datetime.now()
        self.e_has_more = True  # Track if E has more chunks
        self.f_has_more = True  # Track if F has more chunks


class RequestMappingManager(CleanupManager):
    """
    Common request mapping manager for leader servers
    """

    def __init__(self):
        super().__init__()
        self._mappings = {}
        self._mappings_lock = threading.Lock()

    def store_mapping(self, leader_request_id, worker_request_id=None, worker_e_request_id=None, worker_f_request_id=None):
        """Store a request mapping - supports both single and dual worker scenarios"""
        with self._mappings_lock:
            self._mappings[leader_request_id] = RequestMapping(
                worker_request_id=worker_request_id,
                worker_e_request_id=worker_e_request_id,
                worker_f_request_id=worker_f_request_id
            )

    def get_worker_request_id(self, leader_request_id):
        """Get worker request ID for a leader request ID (single worker)"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                mapping.last_access = datetime.now()
                return mapping.worker_request_id
        return None

    def get_worker_e_request_id(self, leader_request_id):
        """Get Worker E request ID for Server D"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                mapping.last_access = datetime.now()
                return mapping.worker_e_request_id
        return None

    def get_worker_f_request_id(self, leader_request_id):
        """Get Worker F request ID for Server D"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                mapping.last_access = datetime.now()
                return mapping.worker_f_request_id
        return None

    def update_e_has_more(self, leader_request_id, has_more):
        """Update whether Worker E has more chunks"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                mapping.e_has_more = has_more

    def update_f_has_more(self, leader_request_id, has_more):
        """Update whether Worker F has more chunks"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                mapping.f_has_more = has_more

    def get_e_has_more(self, leader_request_id):
        """Get whether Worker E has more chunks"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            return mapping.e_has_more if mapping else False

    def get_f_has_more(self, leader_request_id):
        """Get whether Worker F has more chunks"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            return mapping.f_has_more if mapping else False

    def has_more_chunks(self, leader_request_id):
        """Check if either worker has more chunks"""
        with self._mappings_lock:
            mapping = self._mappings.get(leader_request_id)
            if mapping:
                return mapping.e_has_more or mapping.f_has_more
        return False

    def remove_mapping(self, leader_request_id):
        """Remove a mapping"""
        with self._mappings_lock:
            self._mappings.pop(leader_request_id, None)

    def _cleanup_expired_items(self):
        """Clean up expired mappings"""
        now = datetime.now()
        with self._mappings_lock:
            expired_keys = [
                req_id for req_id, mapping in self._mappings.items()
                if now - mapping.last_access > self.expiry_time
            ]

            for req_id in expired_keys:
                print(f"Cleaning up expired mapping: {req_id}")
                del self._mappings[req_id]


class SessionManager(CleanupManager):
    """
    Session manager for worker servers (like Server C, E, F)
    Manages data streaming sessions with chunking
    """

    def __init__(self, chunk_size=5):
        super().__init__()
        self.chunk_size = chunk_size
        self._sessions = {}
        self._sessions_lock = threading.Lock()

    def create_session(self, request_id, data):
        """Create a new session for streaming data"""
        with self._sessions_lock:
            self._sessions[request_id] = {
                'data': data,
                'index': 0,
                'created_at': datetime.now()
            }

    def get_next_chunk(self, request_id):
        """Get next chunk of data for the session"""
        with self._sessions_lock:
            session = self._sessions.get(request_id)
            if not session:
                raise ValueError(f"Session not found: {request_id}")

            start_idx = session['index']
            end_idx = start_idx + self.chunk_size
            chunk_data = session['data'][start_idx:end_idx]

            # Update session index
            session['index'] = end_idx
            session['created_at'] = datetime.now()  # Update access time

            has_more_chunks = session['index'] < len(session['data'])

            # Auto-cleanup if no more chunks
            if not has_more_chunks:
                del self._sessions[request_id]

            return chunk_data, has_more_chunks

    def cancel_request(self, request_id):
        """Cancel a request/session"""
        with self._sessions_lock:
            if request_id in self._sessions:
                del self._sessions[request_id]
                return True
        return False

    def _cleanup_expired_items(self):
        """Clean up expired sessions"""
        now = datetime.now()
        with self._sessions_lock:
            expired_keys = [
                req_id for req_id, session in self._sessions.items()
                if now - session['created_at'] > self.expiry_time
            ]

            for req_id in expired_keys:
                print(f"Cleaning up expired session: {req_id}")
                del self._sessions[req_id]