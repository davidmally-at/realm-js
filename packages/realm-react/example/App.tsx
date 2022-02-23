////////////////////////////////////////////////////////////////////////////
//
// Copyright 2021 Realm Inc.
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

import React, { useCallback, useMemo } from "react";
import { SafeAreaView, View, StyleSheet, Button, Text } from "react-native";

import TaskContext, { Task } from "./app/models/Task";
import IntroText from "./app/components/IntroText";
import AddTaskForm from "./app/components/AddTaskForm";
import TaskList from "./app/components/TaskList";
import colors from "./app/styles/colors";

const { useRealm, useQuery } = TaskContext;

type AppPropsBase = {
  syncEnabled: boolean;
};

type AppPropsSyncDisabled = AppPropsBase & {
  syncEnabled: false;
};

type AppPropsSyncEnabled = AppPropsBase & {
  syncEnabled: true;
  onLogin?: () => void;
  onLogout?: () => void;
  currentUserId: string;
  currentUserName: string;
};

type AppProps = AppPropsSyncDisabled | AppPropsSyncEnabled;

export function App(props: AppProps) {
  const realm = useRealm();
  const result = useQuery(Task);

  const tasks = useMemo(() => result.sorted("createdAt"), [result]);

  const handleAddTask = useCallback(
    (description: string): void => {
      if (!description) {
        return;
      }

      // Everything in the function passed to "realm.write" is a transaction and will
      // hence succeed or fail together. A transcation is the smallest unit of transfer
      // in Realm so we want to be mindful of how much we put into one single transaction
      // and split them up if appropriate (more commonly seen server side). Since clients
      // may occasionally be online during short time spans we want to increase the probability
      // of sync participants to successfully sync everything in the transaction, otherwise
      // no changes propagate and the transaction needs to start over when connectivity allows.
      realm.write(() => {
        realm.create("Task", Task.generate(props.syncEnabled ? props.currentUserId : undefined, description));
      });
    },
    [realm],
  );

  const handleToggleTaskStatus = useCallback(
    (task: Task): void => {
      realm.write(() => {
        // Normally when updating a record in a NoSQL or SQL database, we have to type
        // a statement that will later be interpreted and used as instructions for how
        // to update the record. But in RealmDB, the objects are "live" because they are
        // actually referencing the object's location in memory on the device (memory mapping).
        // So rather than typing a statement, we modify the object directly by changing
        // the property values. If the changes adhere to the schema, Realm will accept
        // this new version of the object and wherever this object is being referenced
        // locally will also see the changes "live".
        task.isComplete = !task.isComplete;
      });

      // Alternatively if passing the ID as the argument to handleToggleTaskStatus:
      // realm?.write(() => {
      //   const task = realm?.objectForPrimaryKey('Task', id); // If the ID is passed as an ObjectId
      //   const task = realm?.objectForPrimaryKey('Task', Realm.BSON.ObjectId(id));  // If the ID is passed as a string
      //   task.isComplete = !task.isComplete;
      // });
    },
    [realm],
  );

  const handleDeleteTask = useCallback(
    (task: Task): void => {
      realm.write(() => {
        realm.delete(task);

        // Alternatively if passing the ID as the argument to handleDeleteTask:
        // realm?.delete(realm?.objectForPrimaryKey('Task', id));
      });
    },
    [realm],
  );

  return (
    <SafeAreaView style={styles.screen}>
      <View style={styles.content}>
        <AddTaskForm onSubmit={handleAddTask} />
        {tasks.length === 0 ? (
          <IntroText />
        ) : (
          <TaskList tasks={tasks} onToggleTaskStatus={handleToggleTaskStatus} onDeleteTask={handleDeleteTask} />
        )}
      </View>

      {props.syncEnabled && (
        <>
          {props.onLogin && <Button title="Login" onPress={props.onLogin} />}
          {props.onLogout && <Button title="Logout" onPress={props.onLogout} />}
          {props.currentUserName && <Text style={styles.username}>Logged in as {props.currentUserName}</Text>}
        </>
      )}
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  screen: {
    flex: 1,
    backgroundColor: colors.darkBlue,
  },
  content: {
    flex: 1,
    paddingTop: 20,
    paddingHorizontal: 20,
  },
  username: {
    color: colors.gray,
    textAlign: "center",
  },
});
