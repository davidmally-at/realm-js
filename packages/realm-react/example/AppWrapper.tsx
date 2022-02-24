////////////////////////////////////////////////////////////////////////////
//
// Copyright 2022 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

import React, { useEffect, useRef, useState } from "react";
import { Realm } from "@realm/react";

import TaskContext from "./app/models/Task";
import { App } from "./App";
import LoginScreen from "./app/components/LoginScreen";
import { SYNC_CONFIG } from "./app/config/sync";
import { StyleSheet, View } from "react-native";
import colors from "./app/styles/colors";

export enum AuthState {
  None,
  Loading,
  LoginError,
  RegisterError,
}

function AppWrapperImpl() {
  const { RealmProvider } = TaskContext;

  // If sync is disabled, create the app without any sync functionality
  if (!SYNC_CONFIG.enabled) {
    return (
      <RealmProvider>
        <App syncEnabled={false} />
      </RealmProvider>
    );
  }

  // Set up the Realm app
  const app = useRef(new Realm.App({ id: SYNC_CONFIG.realmAppId })).current;

  // Store the logged in user in state so that we know when to render the login screen and
  // when to render the app. This will be null the first time you start the app, but on future
  // startups, the logged in user will persist.
  const [user, setUser] = useState<Realm.User | null>(app.currentUser);

  const [authState, setAuthState] = useState(AuthState.None);
  const [authVisible, setAuthVisible] = useState(false);

  const handleLogin = async (email: string, password: string) => {
    setAuthState(AuthState.Loading);

    const credentials = Realm.Credentials.emailPassword(email, password);

    try {
      setUser(await app.logIn(credentials));
      setAuthVisible(false);
      setAuthState(AuthState.None);
    } catch (e) {
      console.log("Error logging in", e);
      setAuthState(AuthState.LoginError);
    }
  };

  const handleRegister = async (email: string, password: string) => {
    setAuthState(AuthState.Loading);

    try {
      // Register the user...
      await app.emailPasswordAuth.registerUser({ email, password });
      // ...then login with the newly created user
      const credentials = Realm.Credentials.emailPassword(email, password);
      setUser(await app.logIn(credentials));
      setAuthVisible(false);
      setAuthState(AuthState.None);
    } catch (e) {
      console.log("Error registering", e);
      setAuthState(AuthState.RegisterError);
    }
  };

  const handleLogout = () => {
    setUser(null);
    app.currentUser?.logOut();
  };

  const handleShowAuth = () => {
    setAuthVisible(true);
  };

  useEffect(() => {
    if (user || !SYNC_CONFIG.anonymousAuthEnabled) return;

    (async () => {
      const credentials = Realm.Credentials.anonymous();
      try {
        setUser(await app.logIn(credentials));
      } catch (e) {
        console.log("Error logging in anonymous user", e);
        throw e;
      }
    })();
  }, [user]);

  // Return null while we wait for anonymous login to complete
  if ((!user || !app.currentUser) && SYNC_CONFIG.anonymousAuthEnabled) return null;

  // If we are not logged in, or the user has pressed "Login" as an anonymous user,
  // show the login screen
  if (authVisible || !user || !app.currentUser) {
    return <LoginScreen onLogin={handleLogin} onRegister={handleRegister} authState={authState} />;
  }

  // If we are logged in, add the sync configuration the the Realm and render the app
  return (
    <RealmProvider sync={{ user, partitionValue: app.currentUser.id }}>
      <App
        syncEnabled={true}
        onLogin={SYNC_CONFIG.anonymousAuthEnabled && user.providerType === "anon-user" ? handleShowAuth : undefined}
        onLogout={user.providerType === "anon-user" ? undefined : handleLogout}
        currentUserId={app.currentUser.id}
        currentUserName={user.providerType === "anon-user" ? "Anonymous" : app.currentUser.profile.email}
      />
    </RealmProvider>
  );
}

export default function AppWrapper() {
  // Wrap the app with a background colour to prevent a flash of white while sync is
  // initialising causing RealmProvider to return null
  return (
    <View style={styles.screen}>
      <AppWrapperImpl />
    </View>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
    backgroundColor: colors.darkBlue,
  },
});
