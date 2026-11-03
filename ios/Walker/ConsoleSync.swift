import Foundation

// Session tokens identify a handoff; current legacy console HTTP does not encrypt it.
private final class NoRedirect: NSObject, URLSessionTaskDelegate, @unchecked Sendable {
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) { completionHandler(nil) }
}
enum ConsoleSync {
    static func validIP(_ host: String) -> Bool {
        let parts = host.split(separator: ".", omittingEmptySubsequences: false)
        return parts.count == 4 && parts.allSatisfy { !$0.isEmpty && $0.allSatisfy(\.isNumber) && (0...255).contains(Int($0) ?? -1) }
    }
    static func exchange(host: String, token: String, payload: Data?, trainerKey: String? = nil) async throws -> Data {
        guard validIP(host), token.count == 32, token.allSatisfy({ $0.isHexDigit }), payload == nil || payload?.count == 65536 else { throw WalkerError.message("Enter the console's IPv4 address, without a port.") }
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = 30
        configuration.timeoutIntervalForResource = 45
        configuration.urlCache = nil
        configuration.requestCachePolicy = .reloadIgnoringLocalCacheData
        let session = URLSession(configuration: configuration, delegate: NoRedirect(), delegateQueue: nil)
        defer { session.invalidateAndCancel() }
        var failure: Error = WalkerError.message("Sync failed.")
        for attempt in 0..<3 {
            do {
                var request = URLRequest(url: URL(string: "http://\(host):30820/walker")!)
                request.httpMethod = payload == nil ? "GET" : "POST"
                request.httpBody = payload
                request.setValue(token, forHTTPHeaderField: "X-Walker-Session")
                if let trainerKey, !trainerKey.isEmpty { request.setValue(trainerKey, forHTTPHeaderField: "X-Walker-Trainer") }
                request.setValue("identity", forHTTPHeaderField: "Accept-Encoding")
                request.setValue("close", forHTTPHeaderField: "Connection")
                let (body, raw) = try await session.data(for: request)
                guard let response = raw as? HTTPURLResponse, body.count <= 65536,
                      Int(response.value(forHTTPHeaderField: "Content-Length") ?? "") == body.count else { throw WalkerError.message("Incomplete sync response.") }
                guard response.statusCode == 200 else { throw WalkerError.rejected(response.statusCode, String(data: body, encoding: .utf8) ?? "Console refused sync.") }
                guard response.value(forHTTPHeaderField: "X-Walker-Session") == token else { throw WalkerError.message("Sync session does not match.") }
                if payload == nil {
                    guard body.count == 65536, response.value(forHTTPHeaderField: "X-Walker-CRC32")?.lowercased() == WalkerCore.crc(body), WalkerCore.walkable(body) else { throw WalkerError.message("Received walker failed verification.") }
                    if let trainerKey, !trainerKey.isEmpty, WalkerCore.text(body, 4) != trainerKey { throw WalkerError.message("Trainer name or ID does not match this profile. Recover the console handoff and select the matching profile.") }
                } else if let payload {
                    let expected = WalkerCore.crc(payload)
                    let headerOK = response.value(forHTTPHeaderField: "X-Walker-Stored-CRC32")?.trimmingCharacters(in: .whitespacesAndNewlines).lowercased() == expected
                    let receipt = String(data: body, encoding: .ascii)?.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
                    guard headerOK || receipt == "pwreturn1 \(token) \(expected)" else { throw WalkerError.message("The saved return checksum was missing or did not match. Keep the console on Phone sync and retry. Your phone copy remains locked.") }
                }
                return body
            } catch let error as WalkerError {
                if case .rejected = error { throw error }
                failure = error
            } catch { failure = error }
            if attempt < 2 { try await Task.sleep(nanoseconds: 750_000_000) }
        }
        throw failure
    }
}
