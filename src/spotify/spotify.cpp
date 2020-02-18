#include "spotify.hpp"

using namespace spt;

Spotify::Spotify()
{
	lastAuth = new QDateTime();
	networkManager = new QNetworkAccessManager();
}

Spotify::~Spotify()
{
	delete lastAuth;
	delete networkManager;
}

QNetworkRequest Spotify::request(QString &url)
{
	// Prepare request
	QNetworkRequest request((QUrl("https://api.spotify.com/v1/" + url)));
	// Set header
	request.setRawHeader("Authorization", ("Bearer " + Settings().accessToken()).toUtf8());
	// Return prepared header
	return request;
}

QJsonDocument Spotify::get(QString &url)
{
	// Send request
	auto reply = networkManager->get(request(url));
	// Wait for request to finish
	while (!reply->isFinished())
		QCoreApplication::processEvents();
	// Parse reply as json
	QJsonParseError jsonError{};
	auto json = QJsonDocument::fromJson(reply->readAll(), &jsonError);
	reply->deleteLater();
	if (json.isNull())
		qWarning() << "warning: there was an error parsing json:" << jsonError.errorString();
	// Return parsed json
	return json;
}

void Spotify::put(QString &url, QVariantMap &body)
{
	// Set in header we're sending json data
	auto req = request(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/json"));
	// Send the request, we don't expect any response
	networkManager->put(req, QJsonDocument::fromVariant(body).toJson());
}

bool Spotify::auth()
{
	// Check if we already have access/refresh token
	Settings settings;
	if (settings.accessToken().length() > 0 && settings.refreshToken().length() > 0)
	{
		qDebug() << "access/refresh token already set, refreshing access token";
		return refresh();
	}
	// Check environmental variables
	auto id = clientId();
	auto secret = clientSecret();
	if (id.isEmpty())
		qWarning() << "warning: SPOTIFY_QT_ID is not set";
	if (secret.isEmpty())
		qWarning() << "warning: SPOTIFY_QT_SECRET is not set";
	// Scopes for request, for clarity
	// For now, these are identical to spotify-tui
	QStringList scopes = {
		"playlist-read-collaborative",
		"playlist-read-private",
		"playlist-modify-private",
		"playlist-modify-public",
		"user-follow-read",
		"user-follow-modify",
		"user-library-modify",
		"user-library-read",
		"user-modify-playback-state",
		"user-read-currently-playing",
		"user-read-playback-state",
		"user-read-private",
		"user-read-recently-played"
	};
	// Prepare url and open browser
	QUrl redirectUrl("http://localhost:8888");
	auto authUrl = QString(
		"https://accounts.spotify.com/authorize?client_id=%1&response_type=code&redirect_uri=%2&scope=%3")
			.arg(id)
			.arg(QString(redirectUrl.toEncoded()))
			.arg(scopes.join("%20"));
	// Start a temporary web server to respond to request
	// TODO: Qt currently doesn't support this, just open a browser for now
	QDesktopServices::openUrl(authUrl);
	// Temporary dialog for entering code
	auto code = QInputDialog::getText(
		nullptr,
		"Enter auth code",
		"Enter code from query parameter:");
	if (code.isEmpty())
		return false;
	// Prepare form to send
	auto postData = QString("grant_type=authorization_code&code=%1&redirect_uri=%2")
		.arg(code)
		.arg(QString(redirectUrl.toEncoded()))
		.toUtf8();
	// Prepare request
	QNetworkRequest request(QUrl("https://accounts.spotify.com/api/token"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setRawHeader(
		"Authorization",
		"Basic " + QString("%1:%2").arg(id).arg(secret).toUtf8().toBase64());
	// Send request
	auto reply = networkManager->post(request, postData);
	// Wait for response
	while (!reply->isFinished())
		QCoreApplication::processEvents();
	auto jsonData = QJsonDocument::fromJson(reply->readAll()).object();
	reply->deleteLater();
	if (jsonData.contains("error_description"))
	{
		qWarning() << "warning: failed to get tokens from code:" << jsonData["error_description"];
		return false;
	}
	// Save access/refresh token to settings
	*lastAuth = QDateTime::currentDateTime();
	auto accessToken = jsonData["access_token"].toString();
	auto refreshToken = jsonData["refresh_token"].toString();
	settings.setAccessToken(accessToken);
	settings.setRefreshToken(refreshToken);
	// Everything hopefully went fine
	return true;
}

bool Spotify::refresh()
{
	// Make sure we have a refresh token
	Settings settings;
	auto refreshToken = settings.refreshToken();
	if (refreshToken.isEmpty())
	{
		qWarning() << "warning: attempt to refresh without refresh token";
		return false;
	}
	// Create form
	auto postData = QString("grant_type=refresh_token&refresh_token=%1")
		.arg(refreshToken)
		.toUtf8();
	// Create request
	QNetworkRequest request(QUrl("https://accounts.spotify.com/api/token"));
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	request.setRawHeader(
		"Authorization",
		"Basic " + QString("%1:%2").arg(clientId()).arg(clientSecret()).toUtf8().toBase64());
	// Send request
	auto reply = networkManager->post(request, postData);
	while (!reply->isFinished())
		QCoreApplication::processEvents();
	// Parse json
	auto json = QJsonDocument::fromJson(reply->readAll()).object();
	reply->deleteLater();
	// Check if error
	if (json.contains("error_description"))
	{
		qWarning() << "warning: failed to refresh token:" << json["error_description"];
		return false;
	}
	// Save as access token
	*lastAuth = QDateTime::currentDateTime();
	auto accessToken = json["access_token"].toString();
	settings.setAccessToken(accessToken);
	return true;
}

QString Spotify::clientId()
{
	return QProcessEnvironment::systemEnvironment().value("SPOTIFY_QT_ID");
}

QString Spotify::clientSecret()
{
	return QProcessEnvironment::systemEnvironment().value("SPOTIFY_QT_SECRET");
}